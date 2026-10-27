// NativeSmash error reports: tools/send_report.ps1 posts here after a game that crashed or had online errors, and this files
// the report as an issue in the project's private reports repository. The GitHub key lives only here (the site's
// GITHUB_TOKEN setting: a fine-grained token that can only write issues there); the game and its players never hold it.
// The report arrives already cleaned of names and addresses on the player's PC.

const REPO = process.env.REPORTS_REPO || 'gomeztorressandygabriel-cloud/NativeSmash-reports';
const MAX = 120000;

const text = (v, n) => String(v ?? '').slice(0, n);
const block = (s) => '```\n' + String(s).replace(/```/g, "'''") + '\n```';

export default async (req) => {
  if (req.method !== 'POST') return new Response('POST only', { status: 405 });
  if (req.headers.get('x-nativesmash') !== '1') return new Response('not a NativeSmash report', { status: 403 });
  const raw = await req.text();
  if (raw.length > MAX) return new Response('too big', { status: 413 });
  let r;
  try { r = JSON.parse(raw); } catch { return new Response('not JSON', { status: 400 }); }

  const kind = r.kind === 'online' ? 'online' : 'crash';
  const version = text(r.version, 40) || '?';
  const summary = text(r.summary, 150).replace(/[\r\n]+/g, ' ').trim() || '(no summary)';
  let body = `**Kind:** ${kind}  \n**Version:** ${version}  \n**Windows:** ${text(r.windows, 80)}  \n**Graphics:** ${text(r.gpu, 200)}\n`;
  for (const rep of Array.isArray(r.reports) ? r.reports.slice(0, 3) : [])
    body += `\n### ${text(rep.name, 80)}\n${block(text(rep.text, 15000))}\n`;
  if (r.online_errors) body += `\n### Online errors\n${block(text(r.online_errors, 8000))}\n`;
  if (r.log_tail) body += `\n<details><summary>Last lines of the game's log</summary>\n\n${block(text(r.log_tail, 25000))}\n</details>\n`;
  body = body.slice(0, 64000);

  const res = await fetch(`https://api.github.com/repos/${REPO}/issues`, {
    method: 'POST',
    headers: {
      Authorization: `Bearer ${process.env.GITHUB_TOKEN}`,
      Accept: 'application/vnd.github+json',
      'User-Agent': 'nativesmash-reports',
      'X-GitHub-Api-Version': '2022-11-28',
    },
    body: JSON.stringify({ title: `[${kind}] ${version}: ${summary}`.slice(0, 200), body, labels: [kind] }),
  });
  return new Response(res.ok ? 'ok' : `github ${res.status}`, { status: res.ok ? 200 : 502 });
};

export const config = { path: '/api/report' };
