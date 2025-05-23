import os
import csv
import requests
from io import BytesIO
from PIL import Image, ImageSequence

# ─── CONFIGURE THESE ────────────────────────────────────────────────────────────
CLIENT_ID     = "FILL IN YOUR CLIENT ID HERE"
CLIENT_SECRET = "FILL IN YOUR CLIENT SECRET HERE"

# List all channel login names here
CHANNELS      = [
    "ninja",
    "shroud",
    "summit1g",
    # Add more channels as needed
]

# ────────────────────────────────────────────────────────────────────────────────


BASE_OUTPUT_DIR = "ChannelEmotes"
CSV_PATH        = "channel_emotes.csv"

# API endpoints
TOKEN_URL  = "https://id.twitch.tv/oauth2/token"
USERS_URL  = "https://api.twitch.tv/helix/users"
EMOTES_URL = "https://api.twitch.tv/helix/chat/emotes"


def get_app_access_token(client_id, client_secret):
    """Fetches App Access Token via client credentials."""
    resp = requests.post(
        TOKEN_URL,
        params={
            "client_id": client_id,
            "client_secret": client_secret,
            "grant_type": "client_credentials"
        }
    )
    resp.raise_for_status()
    return resp.json()["access_token"]


def get_user_id(token, username):
    """Resolve a Twitch username to user ID."""
    resp = requests.get(
        USERS_URL,
        headers={
            "Client-ID": CLIENT_ID,
            "Authorization": f"Bearer {token}"
        },
        params={"login": username}
    )
    resp.raise_for_status()
    data = resp.json().get("data", [])
    if not data:
        raise ValueError(f"User '{username}' not found")
    return data[0]["id"]


def fetch_channel_emotes(token, broadcaster_id):
    """Fetch custom emotes for a given broadcaster ID."""
    resp = requests.get(
        EMOTES_URL,
        headers={
            "Client-ID": CLIENT_ID,
            "Authorization": f"Bearer {token}"
        },
        params={"broadcaster_id": broadcaster_id}
    )
    resp.raise_for_status()
    return resp.json().get("data", [])


def save_animated(content: bytes, path: str):
    """Convert animated APNG/WebP to GIF and save."""
    buf = BytesIO(content)
    img = Image.open(buf)
    try:
        frames = [frame.copy() for frame in ImageSequence.Iterator(img)]
    except:
        frames = [img.copy()]
    if len(frames) < 2:
        frames[0].save(path)
        return
    dur = img.info.get("duration", 100)
    durations = [dur] * len(frames)
    frames[0].save(
        path,
        format="GIF",
        save_all=True,
        append_images=frames[1:],
        loop=0,
        duration=durations,
        disposal=2
    )


def main():
    # Authenticate once
    token = get_app_access_token(CLIENT_ID, CLIENT_SECRET)

    os.makedirs(BASE_OUTPUT_DIR, exist_ok=True)
    with open(CSV_PATH, "w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        # Removed Channel column; RowName remains unique identifier
        writer.writerow(["RowName", "Id", "Code", "Texture", "Size"])

        for chan in CHANNELS:
            # resolve user
            try:
                uid = get_user_id(token, chan)
            except Exception as ex:
                print(f"[ERROR] {chan}: {ex}")
                continue

            emotes = fetch_channel_emotes(token, uid)
            out_dir = os.path.join(BASE_OUTPUT_DIR, chan)
            os.makedirs(out_dir, exist_ok=True)

            # Counter for row names
            count = 1
            for e in emotes:
                eid = e["id"]
                name = e["name"]
                # determine format
                fmt = "animated" if "animated" in e.get("format", []) else "static"
                ext = "gif" if fmt == "animated" else "png"
                # choose scale
                scales = e.get("scale", [])
                for desired in ["3.0", "2.0", "1.0"]:
                    if desired in scales:
                        scale = desired
                        break
                else:
                    scale = scales[0] if scales else "1.0"
                # choose theme
                theme = e.get("theme_mode", ["light"])[0]

                # build URL per chat emotes docs
                url = f"https://static-cdn.jtvnw.net/emoticons/v2/{eid}/{fmt}/{theme}/{scale}"

                # download
                try:
                    resp = requests.get(url)
                    resp.raise_for_status()
                except Exception as ex:
                    print(f"[ERROR] {chan} {eid}: download failed - {ex}")
                    continue

                content = resp.content
                filename = f"{eid}.{ext}"
                path = os.path.join(out_dir, filename)

                # save file
                try:
                    if ext == "gif":
                        save_animated(content, path)
                    else:
                        with open(path, "wb") as f:
                            f.write(content)
                except Exception as ex:
                    print(f"[ERROR] {chan} {eid}: save failed - {ex}")
                    continue

                # get dimensions
                try:
                    img = Image.open(BytesIO(content))
                    w, h = img.width, img.height
                except:
                    w = h = 0

                # RowName uses channel and counter
                row_name = f"{chan}_{count}"
                count += 1

                texture_ref = f"/TwitchChat/Emotes/ChannelEmotes/{chan}/{eid}.{eid}"
                size_field = f"(X={w:.6f},Y={h:.6f})"
                writer.writerow([row_name, eid, name, texture_ref, size_field])
                print(f"[{chan}] • {row_name} '{name}' → {w}×{h} ({filename})")

    print("Done! Channel emotes saved into", BASE_OUTPUT_DIR)

if __name__ == "__main__":
    main()
