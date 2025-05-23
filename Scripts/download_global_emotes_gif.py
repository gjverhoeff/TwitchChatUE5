#!/usr/bin/env python3
"""
Download only the highest-quality Twitch global emotes (animated if available),
naming each file as <id>.<ext>, and emit a CSV listing in twitch_emotes/global_emotes.csv.
"""

import os
import csv
import requests

# ─── CONFIGURE THESE ────────────────────────────────────────────────────────────
CLIENT_ID     = "FILL IN YOUR CLIENT ID HERE"
CLIENT_SECRET = "FILL IN YOUR CLIENT SECRET HERE"
# ────────────────────────────────────────────────────────────────────────────────

TOKEN_URL   = "https://id.twitch.tv/oauth2/token"
EMOTES_URL  = "https://api.twitch.tv/helix/chat/emotes/global"
OUTPUT_DIR  = "twitch_emotes"

def get_app_token(client_id, client_secret):
    r = requests.post(
        TOKEN_URL,
        params={
            "client_id": client_id,
            "client_secret": client_secret,
            "grant_type": "client_credentials"
        }
    )
    r.raise_for_status()
    return r.json()['access_token']

def fetch_emotes(client_id, token):
    headers = {
        "Client-ID": client_id,
        "Authorization": f"Bearer {token}"
    }
    params = {"first": 100}
    all_emotes = []
    tpl = None

    while True:
        resp = requests.get(EMOTES_URL, headers=headers, params=params)
        resp.raise_for_status()
        data = resp.json()

        if tpl is None:
            tpl = (
                data['template']
                    .replace("{{id}}", "{id}")
                    .replace("{{format}}", "{format}")
                    .replace("{{theme_mode}}", "{theme_mode}")
                    .replace("{{scale}}", "{scale}")
            )

        all_emotes.extend(data['data'])
        cursor = data.get('pagination', {}).get('cursor')
        if not cursor:
            break
        params['after'] = cursor

    return all_emotes, tpl

def download_best_quality(emotes, tpl, out_dir):
    os.makedirs(out_dir, exist_ok=True)

    for e in emotes:
        eid    = e['id']                        # raw Twitch ID
        fmt    = 'animated' if 'animated' in e.get('format', []) else 'static'
        ext    = 'gif' if fmt == 'animated' else 'png'
        scale  = max(e.get('scale', []), key=float)
        theme  = 'dark' if 'dark' in e.get('theme_mode', []) else e.get('theme_mode', [])[0]

        url    = tpl.format(id=eid, format=fmt, theme_mode=theme, scale=scale)
        fname  = f"{eid}.{ext}"                # just "<id>.<ext>"
        dest   = os.path.join(out_dir, fname)

        try:
            r = requests.get(url)
            r.raise_for_status()
            with open(dest, 'wb') as f:
                f.write(r.content)
            print(f"✓ {fname}")
        except Exception as err:
            print(f"✗ {fname} — {err}")

def write_csv(emotes, out_dir):
    path = os.path.join(out_dir, "global_emotes.csv")
    with open(path, 'w', newline='', encoding='utf-8') as cf:
        w = csv.writer(cf)
        w.writerow(['RowName', 'Id', 'Code', 'Texture', 'Size'])
        for e in emotes:
            eid     = e['id']
            code    = e['name']
            texture = f"/TwitchChat/Emotes/GlobalEmotes/{eid}.{eid}"
            size    = "(X=112.000000,Y=112.000000)"
            w.writerow([eid, eid, code, texture, size])
    print(f"Wrote CSV → {path}")

if __name__ == "__main__":
    token, tpl = get_app_token(CLIENT_ID, CLIENT_SECRET), None
    emotes, tpl = fetch_emotes(CLIENT_ID, token)
    download_best_quality(emotes, tpl, OUTPUT_DIR)
    write_csv(emotes, OUTPUT_DIR)
    print(f"\nDone: {len(emotes)} emotes downloaded (best quality) into ./{OUTPUT_DIR}/")
