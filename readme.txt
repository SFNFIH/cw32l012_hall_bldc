示例目的：
         生成基本的PWM信号，调整占空比和频率。

硬件资源：
          1. CW32L012 StartKit
          2. 系统时钟设置为HSI时钟
          3. 定时器的时钟源设置为系统时钟（HSI）。
              预分频器（PSC）设置为0，表示无预分频。
              自动重载寄存器（ARR）设置为999，表示PWM信号的周期为1000个时钟周期，频率为1kHz。
              4. PA05 作为ATIM的PWM输出
          

演示说明：
         在示波器上观察生成的PWM信号，初始占空比为25%。 每隔1秒，占空比依次调整为25%、50%和75%，观察信号的变化。

使用说明：
+ EWARM
          1. 打开project.eww文件
          2. 编译所有文件：Project->Rebuild all
          3. 载入工程镜像：Project->Debug
          4. 运行程序：Debug->Go(F5)

+ MDK-ARM
          1. 打开project.uvproj文件
          2. 编译所有文件：Project->Rebuild all target files
          3. 载入工程镜像：Debug->Start/Stop Debug Session
          4. 运行程序：Debug->Run(F5)
