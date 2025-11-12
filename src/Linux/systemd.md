# SystemD初使用！

最近有了一个机械硬盘和一些多余的笔记本（ThinkPad X220），想到可以在寝室里面干一些有意思的事（拿来当路由器、存数据），还真可以，那就再跑一些我自己会用到的服务吧！

## FileBrowser

这是我用来处理我的机械硬盘数据的一个程序，它提供了WebUI，启动也非常简单！

但是，他没有提供Systemd的Unit文件，所以就得自己写一个！

```toml
[Unit]
Description=File Browser
After=network.target

[Service]
Type=simple:cite[1]
ExecStart=/usr/local/bin/filebrowser -r "/home/jask/Storage/" -p "8081" -a "0.0.0.0" -d "/home/jask/filebrowser.db"
Restart=on-failure:cite[1]

[Install]
WantedBy=multi-user.target
```

After是启动以来顺序，如果想要强制依赖的话可以使用Requires字段。

Type则是定义服务如何运行的，默认是simple，其他还有oneshot（程序执行一次就退出）、notify（服务会向systemd-notify发送信号）
