# DSAutotune: A "Dead Simple" auto-tune plugin

The goal of this project is to understand how a typical auto-tune plugin works, from the bottom up. These usually have the following parts:

- [x] detecting pitch (currently using [YIN](https://hyuncat.com/blog/yin/))
- [ ] real-time pitch shifting, by comparing the detected pitch to some musical scale

The pitch detector seems to work; I have yet to implement the pitch shifter.
