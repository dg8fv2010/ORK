###ORK-OpenGL Rendering Kernel
####[http://ork.gforge.inria.fr/](http://ork.gforge.inria.fr/)
ORK提供了对OpenGL的C++封装，可以极大简化开发流程。支持OpenGL3.3，部分支持openGL4.0和4.1。但是作者已经很久没有更新了。我在用ORK进行开发的时候，修复了几个bug，并添加了若干帮助函数方便开发。
1. 增加了若干辅助函数
2. 修复了transform feedback的问题
3. 修复了在A卡上运行可能导致崩溃的问题
4. 修复uniform array数组下标的更新问题
5. 增加了对dds文件的支持
6. 添加了已经编译好的GL和pthreads库

需要额外注意的一点就是，在A卡上进行开发需要遵循更加严格的编程标准。shader文件如果是UTF-8带签名的格式，会增加一个标记位，导致A卡无法识别，shader编译失败。