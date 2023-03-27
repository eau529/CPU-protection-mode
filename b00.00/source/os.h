/**
 * 功能：公共头文件
 *
 *创建时间：2022年8月31日
 *作者：李述铜
 *联系邮箱: 527676163@qq.com
 *相关信息：此工程为《从0写x86 Linux操作系统》的前置课程，用于帮助预先建立对32位x86体系结构的理解。整体代码量不到200行（不算注释）
 *课程请见：https://study.163.com/course/introduction.htm?courseId=1212765805&_trace_c_p_k2_=0bdf1e7edda543a8b9a0ad73b5100990
 */
#ifndef OS_H
#define OS_H

#define KERNEL_CODE_SEG     8
#define KERNEL_DATA_SEG    16//在宏定义里设置内核里的代码段和数据段的各自的偏移值（地址多少位）
//CPL和RPL是当前APP处理的段的权限，是应用代码段和应用数据段的最低位
#define APP_CODE_SEG        ((3*8)|3)//"|3"操作将CS的段选择子的最低两位置为1
#define APP_DATA_SEG        ((4*8)|3)
//GDT表中新增TSS描述符表项
#define TASK0_TSS_SEG   (5*8)
#define TASK1_TSS_SEG   (6*8)
//系统调用函数的表项 of GDT表
#define SYSCALL_SEG (7*8)
//GDT表中的LDT表项
#define TASK0_LDT_SEG   ((8*8))
#define TASK1_LDT_SEG   ((9*8))

//LDT表中的表项,要将TI段置1 ,而选择子的RPL（权限字段），因为当前进程运行在特权级=3的情况下，故RPL要置3
//选择子=表项，放在DS数据段寄存器中，去访问GDT表
#define TASK_CODE_SEG (0*8|0x4|3)
#define TASK_DATA_SEG (1*8|0x4|3)

#endif // OS_H

