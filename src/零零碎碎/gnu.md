1. ctrl-d并不是发送信号，而是EOF！ctrl-c发送SIGINT，ctrl-z发送 SIGSTOP 信号，暂停进程执行，进程可以通过 SIGCONT 信号继续运行
