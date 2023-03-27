/**
 * 功能：16位与32位的启动混合代码
 *
 *创建时间：2022年8月31日
 *作者：李述铜
 *联系邮箱: 527676163@qq.com
 *相关信息：此工程为《从0写x86 Linux操作系统》的前置课程，用于帮助预先建立对32位x86体系结构的理解。整体代码量不到200行（不算注释）
 *课程请见：https://study.163.com/course/introduction.htm?courseId=1212765805&_trace_c_p_k2_=0bdf1e7edda543a8b9a0ad73b5100990
 */
	#include "os.h"

	// 声明本地以下符号是全局的，在其它源文件中可以访问
	.global _start,timer_int,syscall_handler

	// 以CPU16位启动实模式，在指定以下的代码生成16位的机器指令，这样才能在启动时的实模式下运行
  	.code16

	// 以下是代码区
 	.text
_start: file:///home/tru34/linux_code/diy-x86os-master/diy-200lines-os/image/disk.img

	//第二章、实模式下，初始化段寄存器，CPU实模式的结构
	mov $0,%ax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%ss
	mov %ax,%gs
	mov %ax,%fs
	mov $_start,%esp //栈底指针


	//第三章、通过中断调用BIOS读取磁盘接口，写入到内存之中(操作系统占内存总的大小：128MB)
read_self_all: //endless loop
	mov $0x7E00,%bx 	//0x7E00-0x7C00=512，磁盘第二个扇区的起始地址是内存中的0x7E00
	mov $0x2,%cx		//开始的扇区号：2
	mov $0x240,%ax  	//ax=AH+AL, AL:0x40,读取磁盘容量:64*512 字节|| AH:0x2, 表示读操作而不是写操作
	mov $0x80,%dx 		//dx：指向第一个磁盘
	int $0x13 			//BIOS的指令0x13，可以通过中断向量表去查询方法的指针，再用指针调用BIOS中具体的方法
	jc read_self_all 	//如果读取失败则跳回循环的flag


	lidt [idt_desc]
	//第四章:开启保护模式的步骤：1.关中断 || 2.初始化储存GDT初始地址的寄存器GDTR 
	//  3.将CR0寄存器的最低位PE置1开启保护模式 || 4.跳转:会清空流水线
	cli
	lgdt [gdt_desc] //往gdtr寄存器加载GDT表，中括号表示读取对应偏移量的字面值
	mov $1,%eax
	lmsw %ax
	jmp $KERNEL_CODE_SEG, $_start_32 //jmp+基地址CS段寄存器+偏移量，使得cs寄存器指向内核代码段


	//第一章、设定磁盘第一个扇区作为引导扇区
	.org 0x1fe 			//org :位置符号 ，这里是512-2=510
	.byte 0x55,0xaa		//让eip寄存器=0x7c00

	//第四章、生成32位指令
	.code32
	.text

//初始化32位指令下，段寄存器的初始值
_start_32:
	mov $KERNEL_DATA_SEG,%ax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%ss
	mov %ax,%gs
	mov %ax,%fs
	mov $_start,%esp //栈底指针

	call os_init //5.2调用c文件中的初始化方法，使得一级页表和二级页表中的具体内容被填充为0x80000000
				 //保护模式的操作系统初始化：5.打开分页机制，6.芯片引脚设置，开启定时中断 4,8,9 GDT表中的段描述符，TSS，调用门 初始化
	
	//第五章、开启分页机制，CPU看到的是虚拟内存，虚拟内存要通过映射表去获得物理内存地址
	mov $pg_dir,%eax
	mov %eax,%cr3//cr3寄存器存一级页表（页目录）pg_dir的地址。CPU只看得到虚拟地址，而虚拟地址实际上从CR3开始通过分页机制转换为物理地址

	mov %cr4,%eax
	orl $(1<<4),%eax
	mov %eax,%cr4//cr4寄存器开启4MB的页的映射

	mov %cr0,%eax
	orl $(1<<31),%eax
	mov %eax,%cr0//cr0寄存器开启分页机制,

	//sti			//开启中断
				//jmp . //项目起始点，jmp前面的是初始化段寄存器

	//切换至特权级=3的c文件中的任务0
	mov $TASK0_TSS_SEG,%ax
	ltr %ax //TR寄存器指向当前任务的TSS描述符，进程状态描述符寄存器
	
	//第十章、初始化ldtr寄存器(8位选择子)，要在进入task0之前初始化
	mov $TASK0_LDT_SEG,%ax
	lldt %ax 

	//第七章、切换低特权级任务
	//模仿中断发生时的寄存器压栈,通过修改SS和CS寄存器，将CPL和RPL置为3：
	push $TASK_DATA_SEG		//SS:存放栈的段地址,存放数据，故指向应用数据段
	push $task0_dpl3_stack+1024*4	//ESP:栈底指针,指向数组末端（高）地址,栈的项从高位压到低位
							//配置特权级=3的描述符指向的栈空间
	push $0x202 //开中断$0x202有问题				//EFLAGS标志寄存器
	push $TASK_CODE_SEG		//CS寄存器
	push $task_0_entry		//EIP寄存器,当前执行函数的地址
	iret 		//中断返回指令，芯片自动做：出栈操作，出栈的值赋给各寄存器
				

timer_int://8253芯片定时触发中断，调用中断处理函数,要通过声明加到c文件中
    //因为al是eax的低位，为了防止发生中断修改了上面代码本来的eax值，故进入中断前要保存eax原来的值（中断保护现场）
	push %ds
	pusha //将通用寄存器压栈
	mov $0x20,%al //往8259主片发送值(只能用al寄存器)，表示中断已经完成
	outb %al,$0x20 //outb al,dx

	mov $KERNEL_DATA_SEG,%ax
	mov %ax,%ds
	
	call task_sched//在中断中（特权级=0的代码）调用任务切换函数

	popa
	pop %ds
	iret

//第九章、增加系统调用
syscall_handler:
	push %ds//系统相关寄存器保存
	pusha

	mov $KERNEL_DATA_SEG,%ax
	mov %ax,%ds //压入（提取参数前）的所有寄存器

	//push color //三个参数的值是由c文件的task0()——> sys_show() ——> 当前汇编函数
	//push str
	//push func
	//这里通过ebp实际栈项的指针ebp往回推,得到task0参数值。然后在将值手动压入栈顶，之后要手动弹出
	mov %esp,%ebp
	push 13*4(%ebp)
	push 12*4(%ebp)
	push 11*4(%ebp)
	call do_syscall	//调用门指向的系统调用函数：字符串输出函数
					//?如何将task0的参数传递给它?
	
	add $(3*4),%esp //手动出栈
	popa
	pop %ds 

	retf $(3*4)		//系统调用时，不能简单的通过ret指令出栈。硬件不会自动弹出三个参数（红色的），要手动弹出
					//因为要从特权级0——>特权级3的栈（状态），故要使用：retf 参数
					//当CPU发现特权级的切换，会在当前任务内自动切换栈

//运行起来是进程，需要栈空间
task_0_entry://任务0特权级==3，要通过iret指令改变原来内核的任务特权级0
	//CS:IP指向任务0入口函数，在任务0中初始化自己程序的数据段寄存器
	//又因为SS堆栈寄存器保存了GDT表中应用数据段的段选择子，故用SS寄存器赋值数据段寄存器
	mov %ss,%ax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%gs
	mov %ax,%fs
	jmp task_0


gdt_desc://word表示16位是界限,long表示32位,因为gdtr的结构=32位基地址+16位界限
	.word(256*8) - 1
	.long gdt_table

//将IDT表加载到IDTR寄存器（32基地址+16位界限）中,8253芯片产生中断就会由IDTR寄存器找寻IDT表的起始位置
idt_desc:
	.word(256*8) - 1
	.long idt_table
