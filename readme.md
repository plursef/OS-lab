## 监听端口和VNC接口
首先在远程机器下载`vncserver`(to provide VNC service)
- `sudo apt update`
- `sudo apt install tightvncserver -y`
装好之后即可在`sakura`这里开一个端口，用来输出VNC内容，同时在我们的机器上开一个`Listening`端口，监听`sakura`这里的VNC内容.
- `sakura:~/6.828/lab$ vncserver :1`
- `password : daisuki`
因为` VNC server running on '127.0.0.1:5900' `, 所以我们在`sakura`的5901端口上开了一个vncserver.  
接下来只要在本地监听就可以了. 

## 在 Mac 上使用 VNC 连接远程机器
- `brew install tiger-vnc` 安装一个tiger-vnc
- `ssh -L 5900:localhost:5900 sakura` 映射端口，因为qemu把自己的vncserver开在了5900端口，26000tcp是用来gdb的，不是让你连vnc的.
- `vncviewer localhost:5900` 连接到本地的5900端口即可看到远程的vnc内容.
- `password : daisuki`
- 如果想要卸载`tiger-vnc`，可以使用`brew uninstall tiger-vnc`命令。然后`brew autoremove`可以清理掉不再需要的依赖包。