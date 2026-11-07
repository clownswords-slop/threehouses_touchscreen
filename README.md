# Three Houses Touchscreen Support

fully opus 5.5 vibecoded mod that adds touchscreen/mouse support to three houses. I'm most of the way through a playthrough as blue lions via ryujinx (w/ a mouse) and covered what I saw. also have to take a break for a short while so I'm uploading a first release in case anyone spots an error I haven't (and for any feedback!)

#### features
- tap to select menu items, tiles, "buttons" that showed contextual controls (e.g. "(L) Log" during dialogue), and info on status screens
- tap-and-drag to control the camera in every situation that allows camera control
- adds a new tab in the options menu labeled "touchscreen" for mod-specific options

#### issues
- cursor navigation through tapping is *mostly* instant but lags behind in a few menus or in any menus left uncovered (it will otherwise guesstimate locations of items)
- most buttons might be too precise/small to actually tap consistently without the use of a mouse (I haven't tested this on an actual switch)
- didn't bother with amiibos or online play so I have no idea if anything's missing with either
- didn't offer a (good) way to scroll up/down menus and you'll likely still want to use the dpad to do so
- not super familiar with switch modding. included is a patched main.npdm which seems standard but maybe conflicts with other mods. it is purely to set permissions for the additional options menu, and can (probably) be excluded if you don't mind the defaults

it looks like a mess under the hood but I didn't bother to thoroughly check (sorry). check the git for the frankly unreadable code and the notes it took with locations of UI elements in memory if they're of any interest, but they're riddled with claude-speak as you might expect. the above was written by me because I'd rather kill myself than make you read claude-speak