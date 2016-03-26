# Introduction #
KillKeys exist so that you can play games without having to worry about accidentally pressing the windows key. The keys essentially becomes dead keys.

This is the reason why KillKeys was created. Since then I have made it possible to specify any key you want. You can now configure KillKeys to block any key you wish.

# Configuration #

You can configure which keys should be blocked in the configuration file called `KillKeys.ini`. The simplest way to open it is through the tray menu.

You can specify keys that should be disabled when not in fullscreen with the `Keys` option. The option `Keys_Fullscreen` applies for fullscreen windows such as games. By default, the windows keys and the menu key are disabled when in fullscreen.

The keys are specified with hex values. You can find hex values by reading [this page](http://msdn.microsoft.com/en-us/library/dd375731(VS.85).aspx) on MSDN.

Here are some common keys:
| **Key**              | **Hex value** |
|:---------------------|:--------------|
| Left Windows key     | 5B            |
| Right Windows key    | 5C            |
| Menu key             | 5D            |
| Capslock             | 14            |

Write a comment if you have a key that you think should be included in this list.

## Language ##

This settings lets you change the language of KillKeys. You can currently choose between English, Spanish and Galician.

## Update ##

KillKeys will by default check if there is a newer version available when it is started. It will only notify you if it finds a new version, it will not download anything for you. If you do not like this behavior, you can disable it here. You can check for update manually in the tray menu, regardless of this setting.


# UAC #

KillKeys does not require administrator privileges. The only case it needs administrator privileges is if it should block keys when an elevated program has keyboard focus. If you installed KillKeys in `C:\Program Files\`, then you need administrator privileges to edit `KillKeys.ini`. The easiest way to fix this is by right clicking on `KillKeys.ini`, click _Properties_, then go to the _Security_ tab and give the _Users_ group _Full control_ to the file.

If you want KillKeys to launch with administrator privileges on startup, then you have to configure the task scheduler to launch KillKeys on log in. Read [this blog post](http://botsikas.blogspot.com/2010/05/autostart-application-that-requires-uac.html) for a tutorial. There is one caveat though, you need to configure **a delay of 30 seconds** before the task is started. Otherwise the tray icon won't be added properly and KillKeys will not work. You must also disable the autostart option in the tray menu. Successfully configuring a task like this will allow KillKeys to be launched on startup with administrator privileges without a UAC prompt.