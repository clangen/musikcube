## Discord Game SDK

> The SDK is currently under extensive development and is subject to change. Suggestions
> about the current API are welcome.

### Setup

- Create an application on the Discord [developer site](https://discordapp.com/developers/applications/me).
- Set a redirect URL. If you don't have one right now, just use <http://127.0.0.1>.
- Enable Rich Presence for the application. This enables whitelist access for the SDK.
  - When you are ready to test with more people, add them to the whitelist.
- Copy the **Client ID**.
  - Use this `CLIENT_ID` when initializing the SDK.
