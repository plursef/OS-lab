## 监听端口和VNC接口
首先在远程机器下载`vncserver`(to provide VNC service)
- `sudo apt update`
- `sudo apt install tightvncserver -y`
装好之后即可在`sakura`这里开一个端口，用来输出VNC内容，同时在我们的机器上开一个`Listening`端口，监听`sakura`这里的VNC内容.
- `sakura:~/6.828/lab$ vncserver :1`
- `password : daisuki`
因为` VNC server running on '127.0.0.1:5900' `, 所以我们在`sakura`的5901端口上开了一个vncserver.  
接下来只要在本地监听就可以了. 