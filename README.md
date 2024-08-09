2024.8.3
gmrender-resurrect  
这个比较古老，fork下来备忘。  
可以安装在nas、树莓、机顶盒等linux系统等设备上，接上音箱，使其作为支持dlna协议音频播放器  
项目介绍主页：https://gmrender.nongnu.org/  
  
linux：apt-get gmediarender  

另一款upmpdcli（基于 MPD 的 UPnP 音频媒体渲染器）：     
https://github.com/GioF71/upmpdcli-docker  
项目介绍主页：https://www.lesbonscomptes.com/upmpdcli/index.html  

让设备支持Airplay相关软件：
比较老：  
Shairplay：https://github.com/juhovh/shairplay  
Shairport：https://github.com/abrasive/shairport  
最新的：  
shairport-sync：https://github.com/mikebrady/shairport-sync  




Headless UPnP Renderer
----------------------

I needed a small headless UPnP media renderer for Linux (for small footprint-use
in a Raspberry Pi or CuBox), but there was not much available.

Found this old project [GMediaRender][orig-project] - but it
was incomplete, several basic features missing and the project has been
abandoned several years ago.

So this is a fork of those sources to resurrect this renderer and add the
missing features to be useful (Original sources in [savannah cvs][orig-cvs]).
To distinguish this project from the original one, this is called
**[gmrender-resurrect](http://github.com/hzeller/gmrender-resurrect)**.

After many features added, this is now a very usable, headless UPnP
renderer that I would consider **stable**. You don't see many changes in this git - don't worry,
this project is not abandoned, but it just works as intended (I use it every day).
If you run into problems, please file bugs, write me directly or send a pull request; I am busy
but will try to respond.

If you have tested gmrender resurrect with your control point, **please add
it to the [Compatibility Wiki][compat-wiki] page** (even if there is a problem).
At this point, it should work with all media controllers, if not,
please file a bug so that we can figure out the issue and I can make it work
for you. See [INSTALL.md](./INSTALL.md) how to create a logfile that helps
to narrow down things.

If you're running this on a Raspberry Pi, you might be interested to connect
a little LCD display. Check out the **[upnp-display][]** github project.

Installation
------------
For installation instructions, see [INSTALL.md](./INSTALL.md)

You can reach me via <h.zeller@acm.org>.


[orig-project]: http://gmrender.nongnu.org/
[orig-cvs]:http://cvs.savannah.gnu.org/viewvc/gmrender/?root=gmrender
[compat-wiki]: https://github.com/hzeller/gmrender-resurrect/wiki/Comptibility
[upnp-display]: https://github.com/hzeller/upnp-display
[open-max-support]: https://github.com/hzeller/gmrender-resurrect/issues/33#issuecomment-23859699
