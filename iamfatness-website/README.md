# iamfatness.us

Umbrella landing site for **iamfatness.us** — the front door that introduces the
studio and links out to each product on its own subdomain:

- **CoreVideo** → https://corevideo.iamfatness.us/ (OBS plugin + CoreVideo Pro)
- **Resonance** → https://resonance.iamfatness.us/ (DJ-style music player / EQ)

> ⚠️ **Staging note:** This folder currently lives on a branch of the
> `CoreVideo` repo only as a delivery mechanism. It is **destined for the
> standalone `iamfatness/IamfatnessWebsite` repo** and is not meant to be merged
> into CoreVideo. See [Moving this into IamfatnessWebsite](#moving-this-into-iamfatnesswebsite).

## What this is

A tiny, dependency-free static site served by a Cloudflare Worker, mirroring the
deployment pattern already used by `corevideo.iamfatness.us` and
`resonance.iamfatness.us`:

```
public/            Static site (served by the Worker's ASSETS binding)
  index.html       Landing page
  404.html         Custom not-found page
  assets/site.css  Brand styling (dark cyan/blue, matches CoreVideo)
  CNAME            iamfatness.us
site-worker.js     Worker: static assets + security headers + trailing slash + 404
wrangler.jsonc     Worker config + routes for iamfatness.us and www.iamfatness.us
.github/workflows/deploy-site.yml   Deploy on push to main (or manual dispatch)
```

There is no build step — `public/` is served as-is.

## Local preview

```sh
npm install
npm run dev      # wrangler dev
```

## Deploy

Deployment goes to the Cloudflare Worker named `iamfatness-website`, bound to the
routes `iamfatness.us/*` and `www.iamfatness.us/*` in the `iamfatness.us` zone.

The `Deploy Site` GitHub Actions workflow runs on pushes to `main` that touch the
site, and can be triggered manually. It requires two repository secrets/vars:

- `CLOUDFLARE_API_TOKEN` (secret) — permission to deploy Worker scripts + assets
- `CLOUDFLARE_ACCOUNT_ID` (secret or variable)

Manual deploy:

```sh
npm install
npm run deploy   # wrangler deploy
```

## Moving this into IamfatnessWebsite

The session that created this could not push to `iamfatness/IamfatnessWebsite`
(it wasn't in that session's repo scope). To publish it:

**Option A — use the helper script** (from inside this folder):

```sh
./push-to-website-repo.sh git@github.com:iamfatness/IamfatnessWebsite.git main
```

It clones the website repo, copies everything here (except the script itself and
build artifacts) into it, commits, and pushes.

**Option B — do it by hand:**

```sh
git clone git@github.com:iamfatness/IamfatnessWebsite.git
rsync -a --exclude '.git/' --exclude 'node_modules/' \
  --exclude 'push-to-website-repo.sh' iamfatness-website/ IamfatnessWebsite/
cd IamfatnessWebsite && git add -A && git commit -m "Add iamfatness.us landing site" && git push
```

After pushing, set the two Cloudflare secrets and run **Deploy Site**, then point
the `iamfatness.us` zone's route at the `iamfatness-website` Worker.

Once the site is live and verified, the `iamfatness-website/` folder can be
deleted from the CoreVideo branch (it was only ever staging).
