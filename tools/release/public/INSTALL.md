# Install SimGolf Remastered

Takes about 5 minutes (plus the HD art download, about 2.6 GB). Windows 10 or 11.

## 1. Get your game files

You need two things. We cannot give them to you, and the installer never
downloads them.

**a) The SimGolf disc image.** Either:
- the `.7z` from the Internet Archive:
  <https://archive.org/details/sid-meiers-sim-golf-1641547> (about 250 MB), or
- an `.iso` / `.bin` of your own SimGolf CD, made with any disc-image tool.

You do not need to unzip it. Setup opens a `.7z` or `.zip` for you.

**b) The version 1.02 no-CD `golf.exe`.** This is usually a zip called
`SIMGOLF.102.ENG.MYTH.NOCD.ZIP`, from Abandonware DOS:
<https://www.abandonwaredos.com/abandonware-game.php?abandonware=Sid+Meier%27s+SimGolf&gid=2317>

It must be exactly version 1.02 with the no-CD fix. Setup checks the file's
fingerprint (SHA-1 `6f05a3ae3b5050ed82dc0010c03842a2e0ab166b`) and tells you if
it is wrong. You do not need to unzip it.

## 2. Download and unzip Remastered

Download `SimGolf-Remastered-v*-windows.zip` from the Releases page. Right-click
the zip, **Extract All**, and open the new folder. (Do not run Setup from
inside the zip; Windows will not let it find its files.)

## 3. Run Setup

Double-click **Setup.cmd**. If Windows says "Windows protected your PC", click
*More info*, then *Run anyway* (the program is not signed).

Setup asks you:
1. Where to install (the default is fine; it needs about 1.5 GB, plus 3 GB for HD art).
2. Your no-CD zip or `golf.exe`.
3. Your disc image.
4. Whether to download the HD art pack. Say **Yes** unless you are short on space
   or on a slow connection. You can add it later: run Setup again and choose
   Yes, or download the pack yourself (below).

Setup checks both files, unpacks the game, and puts a **SimGolf Remastered**
shortcut on your desktop. That is all.

## Adding the HD art later, or downloading it yourself

Go to the pack's page, <https://github.com/finalpantasy/simgolf-hd-art/releases>,
and download every file there (`parts.json` and the `simgolf-hd-art-part*.zip`
files) into one folder. Then run this in PowerShell, with your folder:

```
.\setup.ps1 -Dir "C:\path\to\your\install" -Disc "your disc" -NoCd "your no-CD zip" -Hd "C:\path\to\pack folder"
```

(Setup reinstalls the game too, which is quick. Your saved games are kept.)
Or just unzip the parts into the `art\upscaled` folder of your install, so that
`art\upscaled\index.tsv` exists.

## If something goes wrong

| What you see | What to do |
|---|---|
| "That golf.exe is not the version 1.02 no-CD file" | You have another version. Download `SIMGOLF.102.ENG.MYTH.NOCD.ZIP` as above. |
| "not the SimGolf CD" | The disc image is the wrong game or a damaged copy. Use the Internet Archive `.7z` above. |
| Windows could not open the `.7z` | Install the free 7-Zip, unpack the archive, and choose the `.iso` or `.bin` inside. |
| "Not enough free space" | The HD pack needs about 6 GB free while installing. Free some space or say No to HD art. |
| A download stopped halfway | Run Setup again; it resumes. |
| "damaged (checksum mismatch)" | Delete the file named and run Setup again. |
| The game does not start | Open `golf.log` in the install folder; it says why. Make sure no other copy of the game is running. |
| The picture looks blurry or pixelated | The HD art pack is not installed (run Setup again, Yes to HD art) or Options is set to Original textures. |
| Windows SmartScreen warning | The program is unsigned. *More info*, then *Run anyway*. |

If you are stuck, open an issue on the project page and attach `golf.log`.

## Uninstall

Delete the install folder and the desktop shortcut. Nothing else was changed on your PC.
Your saved games are in `game\Saved Games` inside the install folder; copy them out first if you want to keep them.
