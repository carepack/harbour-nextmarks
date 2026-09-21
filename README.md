# harbour-nextmarks

A native SailfishOS app that saves links to a [Nextcloud Bookmarks](https://github.com/nextcloud/bookmarks)
folder of your choice, via the Sailfish share menu.

This is **not** a sync client -- there is no local bookmark store and no
bidirectional sync. Every "save" is a single one-way write to your
server: share a link, pick a folder, done. (The project started out
named after [Floccus](https://github.com/floccusaddon/floccus), whose
bookmark-syncing idea it borrowed the general spirit of, but "Nextmarks"
fits its much narrower, Nextcloud-only, one-way scope better.)

## Features

- Share a link from any app (browser, etc.) via "Save to Nextcloud Bookmarks"
- Log in using Nextcloud's own browser-based Login Flow v2 -- this app
  never sees your password, and works with 2FA-protected accounts
- Home screen shows your account's folder tree directly, alphabetically
  sorted; tap a folder to browse the bookmarks in it
- Create and delete folders, from the home screen or while picking a
  folder to save into
- Add a bookmark manually from the app itself, or delete one from a
  folder's list
- Bookmarks show their server-side preview image or favicon, when Nextcloud
  has one
- Follows the system language automatically (German and English so far;
  falls back to English for anything else)

## Building

Requires the [Sailfish SDK](https://docs.sailfishos.org/Tools/Sailfish_SDK/Installation/).

```bash
sfdk cmake .
sfdk cmake --build .
sfdk package
```

## Architecture

- `src/nextcloudclient.{h,cpp}` -- Login Flow v2, folder listing/creation/
  deletion, bookmark creation/deletion, all via the Nextcloud Bookmarks
  REST API (`/index.php/apps/bookmarks/public/rest/v2`)
- `src/sharereceiver.{h,cpp}` -- receives shared links via a hand-rolled
  D-Bus receiver, since the stock `Sailfish.Share` QML `ShareProvider`
  doesn't match the resource shape Sailfish Browser actually sends (see
  the comments in `sharereceiver.h` for the reverse-engineered details)
- `src/bookmarkimageprovider.{h,cpp}` -- an async `QQuickImageProvider`
  that fetches a bookmark's preview image/favicon through the same
  authenticated request every other API call uses, since a plain QML
  `Image` can't send a custom auth header
- `translations/` -- German translation (`harbour-nextmarks-de.ts`);
  English is the source language

## License

MIT
