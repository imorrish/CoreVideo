# Zoom Marketplace OAuth setup

CoreVideo uses a Zoom Meeting SDK JWT to initialize the Meeting SDK helper and
uses a user-level OAuth token (Authorization Code + PKCE, public client) to
fetch a short-lived ZAK for attributed joins. This is needed for external-
account meetings and Marketplace review.

Public PKCE is the only OAuth mode CoreVideo supports at runtime: a desktop
binary cannot keep a client secret, so confidential OAuth is intentionally
not available.

## Zoom Marketplace app (publisher, one-time)

1. Create a **General** app in the Zoom App Marketplace.
2. Enable **User-managed** OAuth and configure the app as a **Public Client**
   (PKCE, no client secret).
3. Add this Redirect URL:
   `corevideo://oauth/callback`
4. Add the same value to the OAuth allow list:
   `corevideo://oauth/callback`
5. Add the minimum scopes used by the build:
   `user:read:zak`
   `user:read:user`
6. Keep the Meeting SDK credentials configured for SDK authentication. OAuth
   credentials are separate from Meeting SDK credentials.

## Embedding the Public Client ID into the build (publisher)

The OAuth Public Client ID is part of the published app's identity, not a
per-user setting. CoreVideo bakes it in at compile time:

```
cmake -B build -DZOOM_EMBED_OAUTH_CLIENT_ID=<your_public_client_id> ...
```

In CI, pass the value as a GitHub Actions secret so it never lands in the
source tree. The value is written into `kEmbeddedOAuthClientId` (see
`src/zoom-credentials.h.in`) and read by `ZoomPluginSettings::load()` as the
default for `oauth_client_id`.

Developers can override the embedded value for local testing by adding an
`OAuthClientId=...` entry under `[ZoomPlugin]` in OBS `global.ini`. There is
no UI for this; it exists only as a development escape hatch.

## End-user sign-in

1. Install a CoreVideo build that has the Public Client ID embedded.
2. Open OBS, then open **Tools > Zoom Plugin Settings**.
3. In the **Zoom Account** section click **Sign in with Zoom** and approve the
   app in the browser. There are no Client ID, Client Secret, or
   Authorization URL fields to configure — the build already knows.
4. The callback helper (`CoreVideoOAuthCallback.exe` on Windows,
   `CoreVideoOAuthCallback.app` on macOS) is registered for the
   `corevideo://` URL scheme the first time you click Sign in and forwards the
   redirect to the running plugin on `127.0.0.1:<ControlServerPort>` (default
   `19870`).

## Runtime flow

1. CoreVideo generates a high-entropy `code_verifier`, derives an S256
   `code_challenge`, and opens the system browser at
   `https://zoom.us/oauth/authorize` with the embedded Public Client ID,
   `redirect_uri=corevideo://oauth/callback`, scopes, `state`,
   `code_challenge`, and `code_challenge_method=S256`.
2. Zoom redirects to `corevideo://oauth/callback?...`.
3. The callback helper forwards that URL to the plugin's control server.
4. The plugin verifies `state`, posts to `https://zoom.us/oauth/token` with
   `grant_type=authorization_code`, the `code`, the `redirect_uri`, the
   `code_verifier`, and the Public Client ID as a form field (no Authorization
   header, no client secret), and stores access/refresh tokens.
5. Before joining a meeting, the plugin refreshes the access token if needed
   (same PKCE-style refresh request, `client_id` in the form body) and calls
   `GET https://api.zoom.us/v2/users/me/token?type=zak`.
6. The returned ZAK is passed into the Meeting SDK `JoinParam4WithoutLogin`
   as `userZAK`.

## Security notes

- PKCE (S256) is used on every authorization. The verifier is generated from
  the platform CSPRNG.
- No client secret is shipped in the binary. The settings dialog does not
  expose Client ID, Client Secret, or Authorization URL fields, so users
  cannot misconfigure the integration.
- Windows token storage uses DPAPI before writing tokens into OBS global
  config.
- Refresh tokens are rotated; always persist the latest refresh token Zoom
  returns.
- Windows builds must ship Qt's TLS backend plugins, especially the Schannel
  backend under `obs-plugins/64bit/plugins/tls`, or OAuth HTTPS requests will
  fail before tokens or ZAKs can be fetched.
- The URL callback command bypasses the local control-server token, but the
  OAuth `state` and one-time verifier are still required before any token
  exchange occurs.
- Do not log access tokens, refresh tokens, ZAKs, authorization codes, or
  PKCE verifiers.

## Marketplace review checklist

- Explain that CoreVideo joins meetings as an OBS capture/ISO recording tool.
- Request only the scopes used by the build.
- Provide test credentials and a test meeting hosted outside the app account.
- Document the visible in-product OAuth sign-in and uninstall/disconnect
  path.
- Make sure the Marketplace listing explains when meeting audio/video is
  captured, where it is processed, and that raw media stays local unless OBS
  outputs it.
