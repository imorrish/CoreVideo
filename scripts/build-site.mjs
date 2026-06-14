import fs from "node:fs";
import path from "node:path";

const root = process.cwd();
const wikiDir = path.join(root, "wiki");
const docsDir = path.join(root, "docs");
const siteAssetsDir = path.join(root, "site-assets");
const outDir = path.join(root, "public");

const pages = [
  {
    source: "Home.md",
    title: "CoreVideo",
    description: "CoreVideo: the OBS plugin for live Zoom video, and CoreVideo Pro, a standalone app for producing online conversations.",
    output: "index.html",
  },
  {
    source: "Terms-of-Use.md",
    title: "Terms of Use",
    description: "Terms of Use for the CoreVideo OBS Studio plugin.",
    output: "terms/index.html",
    aliases: ["terms-of-use/index.html", "Terms-of-Use/index.html"],
  },
  {
    source: "Privacy-Policy.md",
    title: "Privacy Policy",
    description: "Privacy Policy for the CoreVideo OBS Studio plugin.",
    output: "privacy/index.html",
    aliases: ["privacy-policy/index.html", "Privacy-Policy/index.html"],
  },
  {
    source: "Support.md",
    title: "Support",
    description: "Support resources for the CoreVideo OBS Studio plugin.",
    output: "support/index.html",
    aliases: ["Support/index.html"],
  },
];

const markdownPages = [
  {
    source: path.join(docsDir, "ZOOM_MARKETPLACE_OAUTH.md"),
    title: "OAuth Setup",
    description: "Zoom Marketplace OAuth setup for CoreVideo.",
    output: "oauth/index.html",
  },
  {
    source: path.join(docsDir, "CORE_PLUGIN_FUNCTIONALITY.md"),
    title: "CoreVideo (OBS Plugin)",
    description: "CoreVideo OBS plugin workflows, control examples, and ISO recording.",
    output: "core-plugin/index.html",
  },
];

const publicDocumentationUrl =
  process.env.COREVIDEO_SITE_URL?.replace(/\/$/, "") || "";

function ensureDir(filePath) {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
}

function escapeHtml(value) {
  return value
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

function inlineMarkdown(value) {
  const code = [];
  const protectedValue = value.replace(/`([^`]+)`/g, (_match, raw) => {
    const index = code.push(`<code>${escapeHtml(raw)}</code>`) - 1;
    return `@@CODE${index}@@`;
  });
  return escapeHtml(protectedValue)
    .replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>")
    .replace(/_([^_]+)_/g, "<em>$1</em>")
    .replace(/\[([^\]]+)\]\(([^)]+)\)/g, (_match, label, href) => {
      const resolved = resolveHref(href);
      return `<a href="${escapeHtml(resolved)}">${label}</a>`;
    })
    .replace(/@@CODE(\d+)@@/g, (_match, index) => code[Number(index)] ?? "");
}

function renderImage(alt, src) {
  return `<figure class="doc-image"><img src="${escapeHtml(src)}" alt="${escapeHtml(alt)}"></figure>`;
}

function resolveHref(href) {
  if (href.startsWith("https://iamfatness.github.io/CoreVideo/#"))
    return `/documentation/#${href.slice("https://iamfatness.github.io/CoreVideo/#".length)}`;
  if (href.startsWith("https://corevideo.iamfatness.us/documentation/#"))
    return `/documentation/#${href.slice("https://corevideo.iamfatness.us/documentation/#".length)}`;
  if (href === "https://corevideo.iamfatness.us/documentation/")
    return "/documentation/";
  if (href === "https://iamfatness.github.io/CoreVideo/")
    return "/documentation/";
  if (href === "Privacy-Policy")
    return "/privacy/";
  if (href === "Terms-of-Use")
    return "/terms/";
  if (href === "Support")
    return "/support/";
  return href;
}

// Replaces Windows-1252 mojibake sequences that appear when UTF-8 wiki content
// is accidentally re-encoded. Using \u escapes to prevent corruption in transit.
function normalizeText(value) {
  return value
    .replaceAll("â€”", "-")
    .replaceAll("â†’", "->")
    .replaceAll("â€¦", "...")
    .replaceAll("ðŸ“–", "")
    .replaceAll("behaviour", "behavior")
    .replaceAll("https://corevideo.iamfatness.us/documentation/", "/documentation/")
    .replaceAll("https://iamfatness.github.io/CoreVideo/", "/documentation/")
    .replaceAll("https://iamfatness.github.io/CoreVideo", "/documentation");
}

function homeContent() {
  return `<section class="hero">
  <figure class="hero-media">
    <img class="hero-logo" src="/assets/corevideo-logo.jpg" alt="CoreVideo">
  </figure>
  <div class="hero-copy">
    <p class="eyebrow">OBS Studio plugin for Zoom Meeting SDK production workflows</p>
    <h1>Live Zoom video, audio, screen share, and interpretation inside OBS.</h1>
    <p class="lede">CoreVideo provides raw participant media routing for broadcast, recording, and ISO-style production workflows while keeping processing local to the operator machine.</p>
    <div class="hero-actions">
      <a class="button primary" href="/documentation/">Read documentation</a>
      <a class="button" href="/core-plugin/">Core plugin guide</a>
      <a class="button" href="https://github.com/iamfatness/CoreVideo">View source</a>
    </div>
  </div>
</section>
<section class="link-grid products" aria-label="CoreVideo products">
  <a href="/core-plugin/"><strong>CoreVideo (OBS Plugin)</strong><span>OBS Studio plugin for live Zoom video, audio, screen share, and ISO recording.</span></a>
  <a href="/pro/"><strong>CoreVideo Pro (Standalone App)</strong><span>A standalone Windows app for producing high-quality online conversations &mdash; multi-scene production, participant management, streaming, recording, and AI auto-direct. No OBS required.</span></a>
</section>
<section class="link-grid" aria-label="CoreVideo resources">
  <a href="/documentation/"><strong>Documentation</strong><span>Architecture, setup, control APIs, and operating notes.</span></a>
  <a href="/core-plugin/"><strong>Core Plugin Guide</strong><span>OBS workflows, participant routing, isolated audio, and ISO recording.</span></a>
  <a href="/pro/"><strong>CoreVideo Pro</strong><span>Standalone production app for live and recorded conversations.</span></a>
  <a href="/terms/"><strong>Terms of Use</strong><span>Marketplace-ready usage terms and license requirements.</span></a>
  <a href="/privacy/"><strong>Privacy Policy</strong><span>Data processing, local storage, and third-party service details.</span></a>
  <a href="/support/"><strong>Support</strong><span>Issue reporting, troubleshooting, and common fixes.</span></a>
</section>`;
}

function proPageContent() {
  return `<section class="hero">
  <figure class="hero-media">
    <img class="hero-logo" src="/pro/images/corevideo-pro-studio.svg" alt="CoreVideo Pro production console with scene list, program preview, and participant roster">
  </figure>
  <div class="hero-copy">
    <p class="eyebrow">Standalone app for online conversation production</p>
    <h1>Produce polished live conversations, no OBS required.</h1>
    <p class="lede">CoreVideo Pro is a standalone Windows app that turns a Zoom call into a multi-scene, multi-camera production &mdash; with participant management, streaming and recording outputs, and an AI auto-director that keeps the show moving.</p>
    <div class="hero-actions">
      <a class="button primary" href="https://github.com/iamfatness/CoreVideoPro">View on GitHub</a>
      <a class="button" href="/core-plugin/">Compare with CoreVideo (OBS Plugin)</a>
    </div>
  </div>
</section>
<section class="link-grid" aria-label="CoreVideo Pro features">
  <a href="https://github.com/iamfatness/CoreVideoPro#current-slice"><strong>Multi-Scene Production</strong><span>Intro, interview, speaker-plus-slides, panel, and closing scene templates with Cut/Fade/Slide transitions and Take.</span></a>
  <a href="https://github.com/iamfatness/CoreVideoPro#current-slice"><strong>Participant Management</strong><span>Live Zoom roster with Host, Presenter, Panelist, and Guest roles, manual scene-slot assignment, and per-participant audio and video controls.</span></a>
  <a href="https://github.com/iamfatness/CoreVideoPro#current-slice"><strong>Streaming &amp; Recording</strong><span>Program and ISO recording, 1080p/4K output profiles, and multi-destination RTMP/NDI/SRT streaming with preflight checks.</span></a>
  <a href="https://github.com/iamfatness/CoreVideoPro#current-slice"><strong>AI Auto-Direct</strong><span>Magic Scene and Set &amp; Forget automatically recommend and take scene layouts from live Zoom activity, so a show can run itself.</span></a>
</section>
<section>
  <h2>How it fits with CoreVideo</h2>
  <p>CoreVideo Pro is a separate, standalone product from the <a href="/core-plugin/">CoreVideo OBS plugin</a>. Both connect to Zoom for raw participant media, but they&apos;re built for different workflows:</p>
  <table>
    <thead><tr><th>Product</th><th>Form factor</th><th>Best for</th></tr></thead>
    <tbody>
      <tr><td><strong>CoreVideo</strong> (OBS Plugin)</td><td>Plugin inside OBS Studio</td><td>Operators who already run shows in OBS and want Zoom participants as native sources, ISO recording, and an Active Speaker Director.</td></tr>
      <tr><td><strong>CoreVideo Pro</strong> (Standalone App)</td><td>Standalone Windows app</td><td>Producers who want a dedicated, ready-to-go console for online conversations &mdash; scenes, participants, outputs, and AI auto-direct in one app, without configuring OBS.</td></tr>
    </tbody>
  </table>
</section>`;
}

function renderTable(lines) {
  const rows = lines
    .filter((line, index) => index !== 1)
    .map((line) =>
      line
        .trim()
        .replace(/^\|/, "")
        .replace(/\|$/, "")
        .split("|")
        .map((cell) => inlineMarkdown(cell.trim())),
    );
  const [head, ...body] = rows;
  return `<table><thead><tr>${head.map((cell) => `<th>${cell}</th>`).join("")}</tr></thead><tbody>${body
    .map((row) => `<tr>${row.map((cell) => `<td>${cell}</td>`).join("")}</tr>`)
    .join("")}</tbody></table>`;
}

function markdownToHtml(markdown) {
  const lines = markdown.replace(/^﻿/, "").replace(/\r\n/g, "\n").split("\n");
  const html = [];
  let list = [];
  let listTag = "ul";
  let paragraph = [];
  let quote = [];
  let table = [];
  let codeBlock = null;

  const flushParagraph = () => {
    if (!paragraph.length) return;
    html.push(`<p>${inlineMarkdown(paragraph.join(" "))}</p>`);
    paragraph = [];
  };
  const flushList = () => {
    if (!list.length) return;
    html.push(`<${listTag}>${list.map((item) => `<li>${inlineMarkdown(item)}</li>`).join("")}</${listTag}>`);
    list = [];
    listTag = "ul";
  };
  const flushQuote = () => {
    if (!quote.length) return;
    html.push(`<blockquote>${quote.map((item) => `<p>${inlineMarkdown(item)}</p>`).join("")}</blockquote>`);
    quote = [];
  };
  const flushTable = () => {
    if (!table.length) return;
    html.push(renderTable(table));
    table = [];
  };
  const flushAll = () => {
    flushParagraph();
    flushList();
    flushQuote();
    flushTable();
  };

  for (const rawLine of lines) {
    const line = rawLine.trimEnd();
    const trimmed = line.trim();

    const fence = /^```([A-Za-z0-9_-]*)\s*$/.exec(trimmed);
    if (fence) {
      if (codeBlock) {
        const languageClass = codeBlock.language ? ` class="language-${escapeHtml(codeBlock.language)}"` : "";
        html.push(`<pre><code${languageClass}>${escapeHtml(codeBlock.lines.join("\n"))}</code></pre>`);
        codeBlock = null;
      } else {
        flushAll();
        codeBlock = { language: fence[1], lines: [] };
      }
      continue;
    }

    if (codeBlock) {
      codeBlock.lines.push(line);
      continue;
    }

    if (!trimmed) {
      flushAll();
      continue;
    }

    if (trimmed.startsWith("|")) {
      flushParagraph();
      flushList();
      flushQuote();
      table.push(trimmed);
      continue;
    }
    flushTable();

    if (trimmed === "---") {
      flushAll();
      html.push("<hr>");
      continue;
    }

    const heading = /^(#{1,6})\s+(.+)$/.exec(trimmed);
    if (heading) {
      flushAll();
      const level = heading[1].length;
      html.push(`<h${level}>${inlineMarkdown(heading[2])}</h${level}>`);
      continue;
    }

    const bullet = /^[-*]\s+(.+)$/.exec(trimmed);
    if (bullet) {
      flushParagraph();
      flushQuote();
      if (list.length && listTag !== "ul") flushList();
      listTag = "ul";
      list.push(bullet[1]);
      continue;
    }

    const ordered = /^\d+\.\s+(.+)$/.exec(trimmed);
    if (ordered) {
      flushParagraph();
      flushQuote();
      if (list.length && listTag !== "ol") flushList();
      listTag = "ol";
      list.push(ordered[1]);
      continue;
    }

    if (list.length && /^\s+/.test(line)) {
      list[list.length - 1] += ` ${trimmed}`;
      continue;
    }

    if (trimmed.startsWith("> ")) {
      flushParagraph();
      flushList();
      quote.push(trimmed.slice(2));
      continue;
    }

    const image = /^!\[([^\]]*)\]\(([^)]+)\)$/.exec(trimmed);
    if (image) {
      flushAll();
      html.push(renderImage(image[1], image[2]));
      continue;
    }

    paragraph.push(trimmed);
  }
  if (codeBlock) {
    const languageClass = codeBlock.language ? ` class="language-${escapeHtml(codeBlock.language)}"` : "";
    html.push(`<pre><code${languageClass}>${escapeHtml(codeBlock.lines.join("\n"))}</code></pre>`);
  }
  flushAll();
  return html.join("\n");
}

function layout(page, content, options = {}) {
  const nav = [
    ["Home", "/"],
    ["CoreVideo Pro", "/pro/"],
    ["Documentation", "/documentation/"],
    ["Core Plugin", "/core-plugin/"],
    ["OAuth", "/oauth/"],
    ["Terms", "/terms/"],
    ["Privacy", "/privacy/"],
    ["Support", "/support/"],
    ["GitHub", "https://github.com/iamfatness/CoreVideo"],
  ];

  const footerText = options.footerText ??
    "CoreVideo is an independent open-source project and is not affiliated with Zoom Video Communications, Inc.";

  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>${escapeHtml(page.title)} | CoreVideo</title>
  <meta name="description" content="${escapeHtml(page.description)}">
  <link rel="stylesheet" href="/assets/site.css">
</head>
<body class="${options.home ? "home-page" : "document-page"}">
  <header class="site-header">
    <a class="brand" href="/"><span class="brand-mark" aria-hidden="true"></span><span>CoreVideo</span></a>
    <nav>${nav.map(([label, href]) => `<a href="${href}">${label}</a>`).join("")}</nav>
  </header>
  <main class="content">
    ${content}
  </main>
  <footer class="site-footer">
    <span>${footerText}</span>
  </footer>
</body>
</html>
`;
}

function writeText(relativePath, content) {
  const filePath = path.join(outDir, relativePath);
  ensureDir(filePath);
  fs.writeFileSync(filePath, content, "utf8");
}

fs.rmSync(outDir, { recursive: true, force: true });

for (const page of pages) {
  const markdown = normalizeText(
    fs.readFileSync(path.join(wikiDir, page.source), "utf8"),
  );
  const isHome = page.output === "index.html";
  const html = layout(page, isHome ? homeContent() : markdownToHtml(markdown), {
    home: isHome,
  });
  writeText(page.output, html);
  for (const alias of page.aliases ?? []) {
    writeText(alias, html);
  }
}

for (const page of markdownPages) {
  const markdown = normalizeText(fs.readFileSync(page.source, "utf8"));
  const html = layout(page, markdownToHtml(markdown));
  writeText(page.output, html);
}

// CoreVideo Pro landing page
writeText(
  "pro/index.html",
  layout(
    {
      title: "CoreVideo Pro",
      description:
        "CoreVideo Pro: a standalone Windows app for producing high-quality online conversations with multi-scene production, participant management, streaming, recording, and AI auto-direct.",
    },
    proPageContent(),
    {
      home: true,
      footerText:
        "CoreVideo and CoreVideo Pro are independent open-source projects and are not affiliated with Zoom Video Communications, Inc.",
    },
  ),
);

// CoreVideo Pro studio illustration
writeText(
  "pro/images/corevideo-pro-studio.svg",
  `<svg xmlns="http://www.w3.org/2000/svg" width="1536" height="1024" viewBox="0 0 1536 1024" role="img" aria-labelledby="title desc">
  <title id="title">CoreVideo Pro studio layout</title>
  <desc id="desc">Illustration of the CoreVideo Pro production console with scene list, program preview, participant roster, and Take/Record/Stream controls.</desc>
  <defs>
    <style>
      .bg{fill:#070914}.panel{fill:#10141d;stroke:#2b3345}.row{fill:#171c28}.row2{fill:#121722}.header{fill:#1a2030}.text{fill:#f7faff;font-family:Segoe UI,Arial,sans-serif}.muted{fill:#9daac2}.cyan{fill:#22e7e8}.red{fill:#ff5c67}.label{font-size:22px;font-weight:700}.small{font-size:17px}.tiny{font-size:14px}.button{fill:#1c2333;stroke:#3a4358}.live{fill:#ff5c67}
    </style>
  </defs>
  <rect class="bg" width="1536" height="1024"/>
  <rect class="header" x="32" y="32" width="1472" height="64" rx="8"/>
  <circle class="cyan" cx="64" cy="64" r="14"/>
  <text class="text label" x="92" y="72">CoreVideo Pro</text>
  <text class="muted small" x="1180" y="72">Zoom Connected</text>
  <circle class="cyan" cx="1160" cy="64" r="7"/>
  <rect class="panel" x="32" y="112" width="260" height="660" rx="8"/>
  <text class="muted tiny" x="52" y="146">SCENES</text>
  <rect class="row" x="48" y="160" width="228" height="56" rx="6" stroke="#22e7e8"/>
  <text class="text small" x="64" y="194">1  Intro</text>
  <rect class="row2" x="48" y="224" width="228" height="56" rx="6"/>
  <text class="text small" x="64" y="258">2  Interview</text>
  <rect class="row" x="48" y="288" width="228" height="56" rx="6"/>
  <text class="text small" x="64" y="322">3  Speaker + Slides</text>
  <rect class="row2" x="48" y="352" width="228" height="56" rx="6"/>
  <text class="text small" x="64" y="386">4  Panel</text>
  <rect class="row" x="48" y="416" width="228" height="56" rx="6"/>
  <text class="text small" x="64" y="450">5  Closing</text>
  <rect class="panel" x="312" y="112" width="844" height="528" rx="8"/>
  <rect class="live" x="332" y="132" width="60" height="28" rx="4"/>
  <text class="text tiny" x="345" y="151">LIVE</text>
  <text class="muted small" x="1080" y="151">1080p60</text>
  <rect class="row" x="332" y="172" width="404" height="220" rx="6"/>
  <text class="text small" x="350" y="380">Sophia Martinez &#xB7; HOST</text>
  <rect class="row2" x="744" y="172" width="392" height="220" rx="6"/>
  <text class="text small" x="762" y="380">David Chen &#xB7; SPEAKER</text>
  <rect class="row" x="332" y="404" width="404" height="216" rx="6"/>
  <text class="text small" x="350" y="608">Jeremy Collins &#xB7; PANELIST</text>
  <rect class="row2" x="744" y="404" width="392" height="216" rx="6"/>
  <text class="cyan small" x="762" y="436">Building what matters next</text>
  <text class="muted tiny" x="762" y="470">CoreVideo Pro</text>
  <rect class="panel" x="1180" y="112" width="324" height="528" rx="8"/>
  <text class="muted tiny" x="1200" y="146">PARTICIPANTS (7)</text>
  <rect class="row" x="1196" y="160" width="292" height="64" rx="6" stroke="#22e7e8"/>
  <text class="text small" x="1212" y="186">David Chen</text>
  <text class="cyan tiny" x="1212" y="208">SPEAKER &#xB7; Talking</text>
  <rect class="row2" x="1196" y="232" width="292" height="64" rx="6"/>
  <text class="text small" x="1212" y="258">Sophia Martinez</text>
  <text class="muted tiny" x="1212" y="280">HOST</text>
  <rect class="row" x="1196" y="304" width="292" height="64" rx="6"/>
  <text class="text small" x="1212" y="330">Jeremy Collins</text>
  <text class="muted tiny" x="1212" y="352">PANELIST</text>
  <rect class="row2" x="1196" y="376" width="292" height="64" rx="6"/>
  <text class="text small" x="1212" y="402">Ava Patel</text>
  <text class="muted tiny" x="1212" y="424">PANELIST</text>
  <rect class="row" x="1196" y="448" width="292" height="64" rx="6"/>
  <text class="text small" x="1212" y="474">Michael Thompson</text>
  <text class="muted tiny" x="1212" y="496">ATTENDEE</text>
  <rect class="row2" x="332" y="660" width="1172" height="56" rx="6"/>
  <text class="muted small" x="352" y="694">&#x201C;...thank you for being here.&#x201D; &#x2014; CC</text>
  <rect class="panel" x="32" y="744" width="1472" height="120" rx="8"/>
  <rect class="button" x="56" y="772" width="220" height="64" rx="8" stroke="#22e7e8"/>
  <text class="cyan small" x="84" y="798">Magic Scene</text>
  <text class="muted tiny" x="84" y="820">AI auto-direct</text>
  <rect class="button" x="296" y="772" width="240" height="64" rx="8"/>
  <text class="text small" x="324" y="798">Set &amp; Forget</text>
  <text class="cyan tiny" x="324" y="820">Automation On</text>
  <rect class="button" x="900" y="772" width="160" height="64" rx="8"/>
  <text class="text small" x="950" y="810">TAKE</text>
  <rect class="button" x="1076" y="772" width="180" height="64" rx="8" stroke="#ff5c67"/>
  <text class="text small" x="1110" y="810">RECORD</text>
  <rect class="button" x="1272" y="772" width="180" height="64" rx="8" stroke="#22e7e8"/>
  <text class="cyan small" x="1308" y="810">STREAM</text>
  <rect class="row2" x="56" y="884" width="1452" height="100" rx="8"/>
  <text class="muted small" x="80" y="918">Program 1080p60 &#xB7; Good</text>
  <text class="muted small" x="420" y="918">Stream 1080p60 &#xB7; 6.0 Mbps &#xB7; Good</text>
  <text class="muted small" x="780" y="918">Record 1080p60 &#xB7; Good</text>
  <text class="muted small" x="1080" y="918">CPU 18%   Mem 42%   Drops 0 (0.0%)</text>
  <text class="muted small" x="80" y="954">Live 00:28:47</text>
</svg>`,
);

const logoSource = path.join(siteAssetsDir, "corevideo-logo.jpg");
if (fs.existsSync(logoSource)) {
  const logoTarget = path.join(outDir, "assets", "corevideo-logo.jpg");
  ensureDir(logoTarget);
  fs.copyFileSync(logoSource, logoTarget);
}

const docsImagesSource = path.join(docsDir, "images");
if (fs.existsSync(docsImagesSource)) {
  fs.cpSync(docsImagesSource, path.join(outDir, "core-plugin", "images"), {
    recursive: true,
  });
}

const docsHtml = fs.readFileSync(path.join(docsDir, "index.html"), "utf8")
  .replaceAll("iamfatness.github.io/CoreVideo", publicDocumentationUrl
    ? new URL("/documentation", publicDocumentationUrl).host + "/documentation"
    : "CoreVideo documentation")
  .replaceAll("https://iamfatness.github.io/CoreVideo/", "/documentation/")
  .replaceAll('href="ZOOM_MARKETPLACE_OAUTH.md"', 'href="/oauth/"');
writeText("documentation/index.html", docsHtml);
writeText("docs/index.html", docsHtml);

writeText("assets/site.css", `:root {
  color-scheme: dark;
  --bg: #070914;
  --bg-2: #101528;
  --panel: rgba(14, 18, 35, 0.88);
  --panel-strong: #11172c;
  --text: #f7faff;
  --muted: #9daac2;
  --line: rgba(125, 239, 255, 0.18);
  --cyan: #22e7e8;
  --blue: #2aa8ff;
  --cyan-soft: rgba(34, 231, 232, 0.14);
}
* { box-sizing: border-box; }
html { min-height: 100%; }
body {
  margin: 0;
  min-height: 100%;
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  background:
    radial-gradient(circle at 28% 18%, rgba(34, 231, 232, 0.16), transparent 26rem),
    radial-gradient(circle at 82% 72%, rgba(42, 168, 255, 0.12), transparent 30rem),
    linear-gradient(145deg, var(--bg), var(--bg-2));
  color: var(--text);
  line-height: 1.6;
}
body::before {
  content: "";
  position: fixed;
  inset: 0;
  pointer-events: none;
  background:
    linear-gradient(rgba(255,255,255,0.025) 1px, transparent 1px),
    linear-gradient(90deg, rgba(255,255,255,0.025) 1px, transparent 1px);
  background-size: 42px 42px;
  mask-image: linear-gradient(to bottom, black, transparent 78%);
}
a { color: var(--cyan); text-decoration-thickness: 1px; text-underline-offset: 3px; }
.site-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 24px;
  padding: 18px clamp(20px, 5vw, 56px);
  border-bottom: 1px solid var(--line);
  background: rgba(7, 9, 20, 0.82);
  backdrop-filter: blur(16px);
  position: sticky;
  top: 0;
  z-index: 10;
}
.brand {
  display: inline-flex;
  align-items: center;
  gap: 10px;
  color: var(--text);
  font-weight: 750;
  font-size: 20px;
  text-decoration: none;
}
.brand-mark {
  position: relative;
  display: inline-block;
  width: 32px;
  height: 32px;
  border-radius: 50%;
  background: linear-gradient(135deg, var(--cyan), var(--blue));
  box-shadow: 0 0 24px rgba(34, 231, 232, 0.35);
}
.brand-mark::before {
  content: "";
  position: absolute;
  inset: 7px 8px;
  background: #07101c;
  border-radius: 50%;
}
.brand-mark::after {
  content: "";
  position: absolute;
  left: 13px;
  top: 9px;
  width: 0;
  height: 0;
  border-top: 7px solid transparent;
  border-bottom: 7px solid transparent;
  border-left: 11px solid var(--cyan);
  filter: drop-shadow(0 0 6px rgba(34, 231, 232, 0.75));
}
nav {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  flex-wrap: wrap;
  gap: 16px;
}
nav a {
  color: var(--muted);
  font-size: 14px;
  text-decoration: none;
}
nav a:hover { color: var(--text); }
.content {
  width: min(980px, calc(100% - 40px));
  margin: 42px auto 56px;
  background: var(--panel);
  border: 1px solid var(--line);
  border-radius: 8px;
  box-shadow: 0 24px 80px rgba(0, 0, 0, 0.42);
  padding: clamp(24px, 5vw, 52px);
  position: relative;
}
.home-page .content {
  width: min(1120px, calc(100% - 40px));
  background: transparent;
  border: 0;
  box-shadow: none;
  padding: 0;
}
.hero {
  display: grid;
  grid-template-columns: minmax(0, 1.02fr) minmax(360px, 0.98fr);
  align-items: center;
  gap: clamp(30px, 5vw, 68px);
  padding: clamp(28px, 6vw, 70px);
  border: 1px solid var(--line);
  border-radius: 8px;
  background:
    linear-gradient(135deg, rgba(9, 13, 27, 0.96), rgba(12, 17, 34, 0.82)),
    radial-gradient(circle at 12% 20%, rgba(34, 231, 232, 0.16), transparent 22rem);
  box-shadow: 0 24px 90px rgba(0, 0, 0, 0.5);
}
.hero-copy { max-width: 620px; }
.hero-media {
  margin: 0;
  position: relative;
}
.hero-media::before {
  content: "";
  position: absolute;
  inset: 14% 18% auto auto;
  width: 42%;
  aspect-ratio: 1;
  border-radius: 50%;
  background: rgba(34, 231, 232, 0.18);
  filter: blur(42px);
}
.hero-logo {
  display: block;
  position: relative;
  width: 100%;
  height: auto;
  border-radius: 8px;
  box-shadow:
    0 22px 80px rgba(0, 0, 0, 0.42),
    0 0 0 1px rgba(125, 239, 255, 0.12);
}
.eyebrow {
  color: var(--cyan);
  margin: 0 0 12px;
  font-size: 13px;
  font-weight: 750;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}
.hero h1 {
  margin: 0 0 18px;
  max-width: 820px;
  font-size: clamp(36px, 6vw, 70px);
  letter-spacing: 0;
}
.lede {
  color: #d5def0;
  max-width: 660px;
  font-size: 18px;
}
.hero-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  margin-top: 28px;
}
.button {
  display: inline-flex;
  align-items: center;
  min-height: 44px;
  padding: 10px 18px;
  border: 1px solid var(--line);
  border-radius: 6px;
  color: var(--text);
  background: rgba(255,255,255,0.06);
  text-decoration: none;
}
.button.primary {
  color: #06111a;
  border-color: transparent;
  background: linear-gradient(135deg, var(--cyan), var(--blue));
  font-weight: 750;
}
.link-grid {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 14px;
  margin-top: 18px;
}
.link-grid.products {
  grid-template-columns: repeat(2, minmax(0, 1fr));
}
.link-grid.products a {
  min-height: 180px;
}
.link-grid a {
  min-height: 160px;
  padding: 20px;
  border: 1px solid var(--line);
  border-radius: 8px;
  background: rgba(17, 23, 44, 0.86);
  text-decoration: none;
}
.link-grid strong {
  display: block;
  color: var(--text);
  font-size: 18px;
  margin-bottom: 8px;
}
.link-grid span { color: var(--muted); }
h1, h2, h3, h4 { line-height: 1.2; margin: 1.6em 0 0.55em; letter-spacing: 0; }
h1 { margin-top: 0; font-size: clamp(34px, 5vw, 52px); }
h2 { font-size: 26px; border-top: 1px solid var(--line); padding-top: 28px; }
h3 { font-size: 21px; color: #dcecff; }
p, ul, table, blockquote { margin: 0 0 18px; }
p, li, td { color: #d6deee; }
ul { padding-left: 24px; }
code {
  background: rgba(34, 231, 232, 0.1);
  border: 1px solid rgba(34, 231, 232, 0.16);
  border-radius: 4px;
  padding: 0.1em 0.35em;
  font-size: 0.92em;
  color: #bafcff;
}
pre {
  overflow-x: auto;
  background: #08111f;
  border: 1px solid var(--line);
  border-radius: 8px;
  padding: 16px;
  margin: 0 0 18px;
}
pre code {
  display: block;
  background: transparent;
  border: 0;
  padding: 0;
  color: #dcecff;
}
.doc-image {
  margin: 0 0 22px;
  border: 1px solid var(--line);
  border-radius: 10px;
  background: #08111f;
  padding: 12px;
}
.doc-image img {
  display: block;
  width: 100%;
  height: auto;
}
table {
  width: 100%;
  border-collapse: collapse;
  font-size: 15px;
}
th, td {
  text-align: left;
  vertical-align: top;
  border: 1px solid var(--line);
  padding: 10px 12px;
}
th { background: rgba(34, 231, 232, 0.08); color: var(--text); }
blockquote {
  border-left: 4px solid var(--cyan);
  padding: 12px 18px;
  background: var(--cyan-soft);
  color: #e7fbff;
}
hr {
  border: 0;
  border-top: 1px solid var(--line);
  margin: 30px 0;
}
.site-footer {
  color: var(--muted);
  font-size: 13px;
  padding: 24px clamp(20px, 5vw, 56px) 40px;
  text-align: center;
}
@media (max-width: 900px) {
  .hero {
    grid-template-columns: 1fr;
  }
  .hero-media { order: -1; }
  .link-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
  .link-grid.products { grid-template-columns: 1fr; }
}
@media (max-width: 700px) {
  .site-header { align-items: flex-start; flex-direction: column; }
  nav { justify-content: flex-start; }
  .content { margin-top: 24px; }
  .hero { min-height: 0; }
  .link-grid { grid-template-columns: 1fr; }
}
`);

writeText("_redirects", `/terms-of-use /terms/ 301
/Terms-of-Use /terms/ 301
/privacy-policy /privacy/ 301
/Privacy-Policy /privacy/ 301
/Support /support/ 301
/docs /documentation/ 301
/ZOOM_MARKETPLACE_OAUTH.md /oauth/ 301
/docs/ZOOM_MARKETPLACE_OAUTH.md /oauth/ 301
`);
writeText("CNAME", "corevideo.iamfatness.us\n");

console.log(`Built CoreVideo site in ${path.relative(root, outDir)}`);
