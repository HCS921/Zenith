# Zenith 

Zenith is a very lightweight file explorer.

I built this because I wanted to see if I could actually make a file explorer from scratch using nothing but pure C and DirectX 11. It's been a massive learning experience, but I want to be super clear: **this is a buggy passion project.**

It is definitely not a replacement for the real Windows File Explorer. It’s got quirks, it might crash if you have 10 million files, and the UI is still a bit "programmer-art" in places.
### Why does this exist?
- I wanted to see how fast I could make filesystem indexing without using a heavy database.
- Data visualization: I wanted a way to actually SEE where my disk space was going in a way that felt fast and responsive.
- No dependencies: I tried to keep it to the bare Windows headers because I like knowing exactly what's happening under the hood.

### What it does (mostly)
- **Fast Indexing**: It crawls your drives and saves a local index. 
- **Folder Sizes**: It tries its best to calculate exactly how many gigabytes are in your folders, even on OneDrive.
- **Preview**: There's a right-side panel that can show you text or images if you click on them.
- **Search**: Since it has an index, searching for file names is pretty quick.

### How to run it
If you're brave enough to try it either download the newest release or you'll need Visual Studio 2022. Just open a Developer Command Prompt, go to this folder, and run:
`scripts\build.bat`

The app will be in the `build/` folder.

### Disclaimer
This is a hobby project. It's provided as-is, and I can't promise it won't be weird on your specific Windows setup. If it breaks, sorry! It's just a thing I made for fun.

---
*MIT License - Feel free to look at the code and tell me all the things I did wrong!*
