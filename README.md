# King's Quest 8 - Mask of Eternity - Windows 11 Compatibility

A compatibility fix for King's Quest 8: Mask of Eternity to run properly on Windows 11 with enhanced video playback support.

## Project Background

This project is based on the original kqmoefix project found at: https://svn.nicode.net/kqmoefix/

The original project was designed to support KQ8 on Windows 10 and earlier versions, but required significant updates for Windows 11 compatibility, particularly for video playback functionality.

## Key Improvements

1. **Updated dgVoodoo Configuration**: Properly configured dgVoodoo2 settings optimized for Windows 11 compatibility
2. **Modernized Video Playback**: Completely reworked video handling system that no longer relies on legacy MCI commands
3. **Re-encoded Video Files**: All video files have been re-encoded using H.264 format, which is natively supported by Windows 11
4. **Enhanced Window Management**: Improved window handling for both legacy Windows Media Player and Windows 11 UWP Media Player

## Installation Instructions

### Prerequisites
- King's Quest 8: Mask of Eternity (latest version 1.03 from GOG recommended)
- Windows 11

### Installation Steps

1. **Download the base game**: Get the latest known KQ8 from GOG (version 1.03) and install it. Don't lanch the game yet!
2. **Download the compatibility fix**: Download [the patch](https://github.com/SegMash/KQ8FixWin11/releases/download/10_25/KQ8_Win11_Patch.zip)
3. **Extract files**: Extract the zip contents to your KQ8 game folder (overwrite all)
4. **Run NO-CD Fix**: Execute `kqmoepfd.cmd` to enable no CD.
5. **Run the system fix**: Run `kq8fix.exe` and check the box `Mask.exe`
6. **Start playing**: Launch the game (Mask.exe) and enjoy! (No need to set compatibility mode!)

## Recommendations

### Media Player Configuration
- **Set default AVI player**: Go to Settings → Default Apps and set Windows Media Player Legacy as the default AVI player
- **Enable Legacy Media Player**: If not available, enable it via Settings → Apps → Optional Features → Add a feature → Media features → Windows Media Player Legacy

### Troubleshooting
- **Mouse cursor issues**: If using the new Windows 11 Media Player and the cursor becomes locked after video playback, click the mouse to release it
- **VLC compatibility**: VLC player works but may introduce noticeable latency during video playback
- **Skip intro videos**: To bypass intro videos, edit `main.kq` file and set `aviPlayed=1`

## Technical Details

This compatibility fix addresses several Windows 11-specific issues:
- Modern video codec support (H.264)
- UWP Media Player window management
- Enhanced window restoration after video playback
- Improved cursor handling for different media players

## Contributing

Feel free to submit issues and enhancement requests. This project aims to preserve the classic King's Quest 8 experience on modern Windows systems.

## License

This project maintains the same licensing as the original kqmoefix project. Please refer to the original source for license details.