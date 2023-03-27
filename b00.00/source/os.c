/**
 * 功能：32位代码，完成多任务的运行
 *
 *创建时间：2022年8月31日
 *作者：李述铜
 *联系邮箱: 527676163@qq.com
 *相关信息：此工程为《从0写x86 Linux操作系统》的前置课程，用于帮助预先建立对32位x86体系结构的理解。整体代码量不到200行（不算注释）
 *课程请见：https://study.163.com/course/introduction.htm?courseId=1212765805&_trace_c_p_k2_=0bdf1e7edda543a8b9a0ad73b5100990
 */
#include "os.h"


typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

//OS内部服务函数：特权级=0，可以操作硬件
void do_syscall(int func,char *str,char color){
    static int row=0;
    
    if(func==2)
    {   unsigned short * dest=(unsigned short *)0xb8000+80*row;
        while(*str){//从屏幕的最左边显示
            *dest++=*str++| (color<<8);
        }
        row=(row>=25)?0:row+1;//输出完一串字符串后，行+1
    }
    for(int i=0;i<0xFFFFF;i++);
}


//接口：系统调用函数，会使用调用门跳到汇编文件，此时特权级=0，又可以在里面call对应的高特权级c文件的函数（比如关中断）
void sys_show(char *str,char color) //调用函数：让cpu通过调用门 跳转到 汇编的系统调用函数
{
    uint32_t addr[]={0,SYSCALL_SEG};//跳转地址为-->选择子：系统调用门of（GDT表项）,会调用本表内特权级=0的内核代码段
    __asm__ __volatile__("push %[color];push %[str];push %[id];lcalll *(%[a])"
    ::[color]"m"(color),[str]"m"(str),[id]"r"(2),[a]"r"(addr));//跳转到汇编文件的syscall_handler函数
    
    // 采用调用门, 这里只支持5个参数
    // 用调用门的好处是会自动将参数复制到内核栈中，这样内核代码很好取参数
    // 而如果采用寄存器传递，取参比较困难，需要先压栈再取
     
}
//第八章、两个任务之间的切换
void task_0(void){
    uint8_t color=0;
    char *str="task a:1234";
    for(;;)
    {
       sys_show(str,color++);
    }

}
void task_1(void){
    uint8_t color=0xff;
    char *str="task a:5678";
    for(;;)
    {
        sys_show(str,color--);
    }
 
}

uint32_t task0_dpl3_stack[1024],task0_dpl0_stack[1024];
uint32_t task1_dpl3_stack[1024],task1_dpl0_stack[1024];

//第十章、使用LDT，创建task0和task1的LDT表实体，并填充其中代码段和数据段表项
struct {uint16_t limit_l, base_l, basehl_attr, base_limit;}task0_ldt_table[256] __attribute__((aligned(8))) = {//_attribute指示符使得gcc编译链接结构体时，能够把它放在8字节对齐的地址处    
    [TASK_CODE_SEG/8]={0XFFFF,0X0000,0XFA00,0X00CF},
    [TASK_DATA_SEG/8]={0XFFFF,0X0000,0XF300,0X00CF},
};
struct {uint16_t limit_l, base_l, basehl_attr, base_limit;}task1_ldt_table[256] __attribute__((aligned(8))) = {//_attribute指示符使得gcc编译链接结构体时，能够把它放在8字节对齐的地址处    
    [TASK_CODE_SEG/8]={0XFFFF,0X0000,0XFA00,0X00CF},
    [TASK_DATA_SEG/8]={0XFFFF,0X0000,0XF300,0X00CF},
};

//第五章、分页机制
//5.1创建页目录表PDE（1索引<----->页的大小4MB）
#define PDE_P  (1<<0)//后面的P表示存在位
#define PDE_W  (1<<1)//后面的W表示读写位
#define PDE_U  (1<<2)//后面的u表示特权位
#define PDE_PS (1<<7)//后面的PS表示是否开启二级页表

#define MAP_ADDR 0x80000000 //定义要映射的虚拟地址，使得它和物理内存中的某块对应
//物理内存中的一个数组,接着用创建一个二级页表，使得虚拟内存的0x80000000可以去映射它
uint8_t map_phy_buffer[4096] __attribute__((aligned(4096)))={0x36};

//二级页表具体内容
uint32_t pg_table[1024] __attribute__((aligned(4096))) = { 
    PDE_U
};


//一级页表（页目录）PED具体内容
uint32_t pg_dir[1024] __attribute__((aligned(4096))) = { //页目录表1024个表项，4KB=4096对齐
    [0] = (0) | PDE_P | PDE_W | PDE_U | PDE_PS, //第一个表项，起始地址0
};

//6.3设置中断描述符表IDT(由下往上，由低位0往高位31，16位作为一个单元)
struct {uint16_t offset_l, selector, attr, offset_h;}idt_table[256] __attribute__((aligned(8))) = {
    //不用初始值
};

//第四章、启动保护模式：设置GDT表
//创建GDT表,其中的表项描述符：由下方四个参数分别代表四部分的16位表述
//下方参数：低16位limit，高16位基地址，上方：低16位属性，高16位,

struct {uint16_t limit_l, base_l, basehl_attr, base_limit;}gdt_table[256] __attribute__((aligned(8))) = {//_attribute指示符使得gcc编译链接结构体时，能够把它放在8字节对齐的地址处    
    //填充GDT单元（段描述符）：内核数据段和内核代码段的具体内容
    [KERNEL_CODE_SEG/8]={0xffff,0x0000,0x9a00,0x00cf},
    [KERNEL_DATA_SEG/8]={0xFFFF,0x0000,0x9200,0x00cf},
    //将代码段的偏移值/8=GDT表中的索引值（下标）,看图得内核代码段是GDT表中的0后面的索引1，它的值是系统提前给定的
    //将GDT表下标[1]作为代码内核段，通过（段寄存器CS低16位）访问该段选择子，其中的4个值表示4个16位字段。
    //其中第一个字段0xffff表示内核代码区的大小界限是4GB，第二个字段0x000表示描述符在内存中的基地址，          

    //第七章、切换至低特权级,设置特权级=3的应用代码段和应用数据段
    // [APP_CODE_SEG/8]={0xffff,0X0000,0Xfa00,0x00cf},
    // [APP_DATA_SEG/8]={0xffff,0X0000,0Xf300,0x00cf},

    //第八章、新增Task0和Task1的TSS(任务状态段)表项
    [TASK0_TSS_SEG/8]={0x68,0,0xe900,0x0},//第二个参数：段基地址在init函数中初始化
    [TASK1_TSS_SEG/8]={0x68,0,0xe900,0x0},

    [SYSCALL_SEG/8]={0x0000,KERNEL_CODE_SEG,0xec03,0},

    //第十章、task0和task1的LDT表项设置
    [TASK0_LDT_SEG/8]={sizeof(task0_ldt_table)-1,0x0,0xe200,0x00cf},
    [TASK1_LDT_SEG/8]={sizeof(task0_ldt_table)-1,0x0,0xe200,0x00cf},

 };

/**
 * @brief 第五章、两个任务的切换
 * 任务0的TSS具体结构
 */
uint32_t task0_tss[] = {
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)task0_dpl0_stack + 4*1024, KERNEL_DATA_SEG , /* 后边不用使用 */ 0x0, 0x0, 0x0, 0x0,
    // cr3页表, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pg_dir,  (uint32_t)task_0/*入口地址*/, 0x202, 0xa, 0xc, 0xd, 0xb, (uint32_t)task0_dpl3_stack + 4*1024/* 栈 */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap(要改为ldt的表项下标)
    TASK_DATA_SEG, TASK_CODE_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK0_LDT_SEG, 0x0,
};
/**
 * @brief 任务1的TSS具体结构
 */
uint32_t task1_tss[] = {
    // prelink, esp0：当中断产生时，程序自己单独的特权级=0的栈空间, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)task1_dpl0_stack + 4*1024, KERNEL_DATA_SEG , /* 后边不用使用 */ 0x0, 0x0, 0x0, 0x0,
    // cr3, eip：此时CPU会从task1开始程序执行, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pg_dir,  (uint32_t)task_1/*入口地址*/, 0x202, 0xa, 0xc, 0xd, 0xb, (uint32_t)task1_dpl3_stack + 4*1024/* 栈 */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    TASK_DATA_SEG, TASK_CODE_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK1_LDT_SEG, 0x0,

};

//第六章、开启定时中断
//arm的芯片可以通过内存去操作，而x86的芯片需要通过“outb”指令操作
//要在c函数中调用汇编格式
void outb(uint8_t data,uint16_t port)//其中port为输入端口，data为输出的中断描述符表IDT偏移值
{
    __asm__ __volatile__("outb %[v],%[p]"::[p]"d"(port),[v]"a"(data));//汇编指令outb,"d"表示dx寄存器

}

void task_sched(void){//中断触发，在特权级=0的状态下运行，通过,任务1和0的互相切换
    static int task_tss=TASK0_TSS_SEG;//全局变量
    task_tss=(task_tss==TASK0_TSS_SEG)?TASK1_TSS_SEG:TASK0_TSS_SEG;
    uint32_t addr[]={0,task_tss};//选择子，类似EIP指针，让程序跳到选择子：表示的程序段
     __asm__ __volatile__("ljmpl *(%[a])"::[a]"r"(addr));//汇编指令outb,"d"表示dx寄存器

}
void syscall_handler(void);//系统调用函数
void timer_int(void);//汇编中定义的中断处理函数
//保护模式的操作系统初始化：5.打开分页机制，6.芯片引脚设置，开启定时中断 4,8,9 GDT表中的段描述符，TSS，调用门 初始化
void os_init(void){
    //第六章、开启定时中断,outb(data,port),其中port为输入端口，data为输出的中断描述符表IDT偏移值
    //0x20是整个芯片未开启状态，0x21是整个芯片开启状态等价于IRQ0脚
    outb(0x11,0x20);
    outb(0x11,0xA0);//8259主片和从片写入0x11表示它们开始初始化

    //将8253芯片从8259地址0x21等价于IRQ0脚，当它产生中断时要从IDT表0x20出开始查找，并以此作为偏移量访问IDT
    outb(0x20,0x21);//主片配置
    outb(0x28,0xA1);//当从片IRQ0脚发生中断时，中断地址0xA1，访问IDT表0x28偏移位置

    //主片和从片互相连接的引脚置为0
    outb(1<<2,0x21);//主片第二个管脚IRQ2连着从片
    outb(2,0xA1);//从片第二个管脚连着主片

    //设置主片和从片的模式
    outb(0x1,0x21);//将主片设置为8086模式，当中断发生时会向CPU发送信号
    outb(0x1,0xA1);
    
    //将8259芯片除了IRQ0引脚的其他引脚都屏蔽(置1)，只用IRQ0引脚接受8253定时器的中断信号
    outb(0xfe,0x21);
    outb(0xff,0xA1);

    //6.2设置8253定时器的时钟周期
    int tmo=1193180/10;//100ms作为时钟周期
    outb(0x36,0x43);    //二进制计数，模式3，通道0
    outb((uint8_t)tmo,0x40);
    outb(tmo>>8,0x40);

    idt_table[0x20].offset_l=(uint32_t)timer_int & 0xFFFF;//IDT的0x20表项是主片发生中断时候，调用线性地址中的timer_int中断处理函数
    idt_table[0x20].offset_h=(uint32_t)timer_int >> 16;
    idt_table[0x20].selector=KERNEL_CODE_SEG;//选择子：GDT表中偏移，+GDTR可以查询出线性的段基地址
    idt_table[0x20].attr=0x8E00;//采用中断门，段存在，32位，DPL访问权限

    //第八章、将gdt表的任务0和任务1的两个TSS表项指向实体TSS结构,即TSS选择子设置具体值
    gdt_table[TASK0_TSS_SEG/8].base_l=(uint16_t)(uint32_t)task0_tss;
    gdt_table[TASK1_TSS_SEG/8].base_l=(uint16_t)(uint32_t)task1_tss;

    //第九章、增加系统调用，这里设置GDT表项：系统调用门的段选择子：系统调用函数
    gdt_table[SYSCALL_SEG/8].limit_l=(uint16_t)(uint32_t)syscall_handler;//在内核代码段（特权级=0）的偏移位置（函数地址）

    //第五章、根据表项硬件结构，初始化各级页表（之间的关系）
    //左移22位，取虚拟地址22~32，作为页目录表的索引值（下标），即512项
    pg_dir[MAP_ADDR >>22]=(uint32_t)pg_table | PDE_P | PDE_W |PDE_U;//将页目录表中第512项的字面值（指向）pagetable二级页表的开头，并将第512表项各个权限进行设置
    //取虚拟地址12~22位作为索引，先左移12位取得12~32位，再用&与操作取得12~22位。即第0项
    pg_table[(MAP_ADDR>>12)&0x3FF]=(uint32_t)map_phy_buffer | PDE_P | PDE_W |PDE_U;//将二级页表的字面值指向（最终数值）：结构体map_phy_buffer的物理地址

    //第十章、LDT使用，GDT中表项指向实体LDT表
    gdt_table[TASK0_LDT_SEG/8].base_l=(uint16_t)(uint32_t)task0_ldt_table;
    gdt_table[TASK1_LDT_SEG/8].base_l=(uint16_t)(uint32_t)task1_ldt_table;

}


