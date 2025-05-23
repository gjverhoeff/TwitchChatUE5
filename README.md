# Twitch Chat for Unreal Engine 5 Editor

This plugin is made to receive Twitch Chat including Emotes into the Unreal Engine Editor and intended to be used for GFX/XR/Broadcast. Not intended to be used for packaged projects. If you want to do that there are other options like the official Twitch Plugin that can be found on their developer portal. 

## Features

- Connect to Twitch with ClientID and Client Secret. Auto generated Access Token and Refresh Token.
- Auto download static versions of the emotes during capture of chat.
- Add GIF's for emotes manually (Python script for downloading Global Emotes and Channel Emotes are included).
- Get chat in Editor Window
- Get Twitch Chat Data in Blueprints

## TODO
- Get auto-download for animated gifs to work on runtime
- Widget Component to display a line of chat
- Text3D Component to display a line of chat

----

## Contribution
This plugin is under MIT license, but if you extent it in any way it would be really appreciated to submit these for possible incorporation. In this way the community using Twitch and Unreal can grow and help each other :) 


---

## Acknowledgement

- [neil3d](https://github.com/neil3d/UAnimatedTexture5) as the UAnimatedTexture5 (including all of its existing functions) are included in the Twitch Chat Plugin. 
- This plugin was not made with vibe coding, but ChatGPT was consulted.