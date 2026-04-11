#include "sd_vendor.h"

#include "radio_vendor.h"

#include <SD.h>
#include <SPI.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace
{
constexpr int kSdCs = 42;
constexpr int kSdSck = 48;
constexpr int kSdMosi = 47;
constexpr int kSdMiso = 41;

bool s_sd_ready = false;
char s_status[96] = "SD pending";
constexpr const char *kMusicRoots[] = {"/songs", "/Songs"};
constexpr size_t kMusicRootCount = sizeof(kMusicRoots) / sizeof(kMusicRoots[0]);
constexpr size_t kMaxMusicTracks = 192;
char s_music_root[64] = "/songs";

struct MusicTrackEntry
{
    char path[128]{};
    char title[96]{};
    char group[48]{};
};

int compare_album_names(const void *lhs, const void *rhs)
{
    return strcmp(static_cast<const char *>(lhs), static_cast<const char *>(rhs));
}

MusicTrackEntry s_music_tracks[kMaxMusicTracks];
int s_music_track_count = 0;
int s_music_current_index = -1;
bool s_music_loaded = false;
char s_music_current_title[96] = "Nothing playing";
char s_music_current_group[48] = "Ukrainian";
char s_music_current_path[128]{};
constexpr size_t kMaxMusicAlbums = 96;
char s_music_albums[kMaxMusicAlbums][48]{};
char s_music_album_keys[kMaxMusicAlbums][64]{};
bool s_music_album_has_children[kMaxMusicAlbums]{};
int s_music_album_count = 0;
int s_music_selected_album = -1; // -1 -> all tracks
char s_music_selected_album_name[48] = "All";
char s_music_selected_album_key[64]{};
char s_music_browser_root[64]{};


const char *kMurmurAllow[] = {
    "murmur_",
};

const char *kSelftalkAllow[] = {
    "selftalk_",
};

const char *kQuestionAllow[] = {
    "questions_",
};

void set_status(const char *fmt, ...);

String trim_track_label(String value)
{
    value.replace('\r', ' ');
    value.replace('\n', ' ');
    value.replace('\t', ' ');
    value.trim();
    if (value.length() > 0 && value.charAt(0) == 0xFEFF)
    {
        value.remove(0, 1);
    }
    while (value.indexOf("  ") >= 0)
    {
        value.replace("  ", " ");
    }
    return value;
}


String transliterate_track_label(String value)
{
    value = trim_track_label(value);
    String out;
    out.reserve(value.length() * 2);

    for (size_t i = 0; i < value.length();)
    {
        const uint8_t b0 = static_cast<uint8_t>(value.charAt(i));
        if (b0 < 0x80)
        {
            out += static_cast<char>(b0);
            ++i;
            continue;
        }

        if (b0 == 0xE2 && i + 2 < value.length())
        {
            i += 3;
            continue;
        }

        if (b0 == 0xD2 && i + 1 < value.length())
        {
            const uint8_t b1 = static_cast<uint8_t>(value.charAt(i + 1));
            if (b1 == 0x90) out += "G";
            else if (b1 == 0x91) out += "g";
            i += 2;
            continue;
        }

        if (i + 1 >= value.length())
        {
            ++i;
            continue;
        }

        const uint8_t b1 = static_cast<uint8_t>(value.charAt(i + 1));
        String repl;

        if (b0 == 0xD0)
        {
            switch (b1)
            {
                case 0x81: repl = "Yo"; break;
                case 0x84: repl = "Ye"; break;
                case 0x86: repl = "I"; break;
                case 0x87: repl = "Yi"; break;
                case 0x90: repl = "A"; break;
                case 0x91: repl = "B"; break;
                case 0x92: repl = "V"; break;
                case 0x93: repl = "H"; break;
                case 0x94: repl = "D"; break;
                case 0x95: repl = "E"; break;
                case 0x96: repl = "Zh"; break;
                case 0x97: repl = "Z"; break;
                case 0x98: repl = "Y"; break;
                case 0x99: repl = "I"; break;
                case 0x9A: repl = "K"; break;
                case 0x9B: repl = "L"; break;
                case 0x9C: repl = "M"; break;
                case 0x9D: repl = "N"; break;
                case 0x9E: repl = "O"; break;
                case 0x9F: repl = "P"; break;
                case 0xA0: repl = "R"; break;
                case 0xA1: repl = "S"; break;
                case 0xA2: repl = "T"; break;
                case 0xA3: repl = "U"; break;
                case 0xA4: repl = "F"; break;
                case 0xA5: repl = "Kh"; break;
                case 0xA6: repl = "Ts"; break;
                case 0xA7: repl = "Ch"; break;
                case 0xA8: repl = "Sh"; break;
                case 0xA9: repl = "Shch"; break;
                case 0xAA: repl = ""; break;
                case 0xAB: repl = "Y"; break;
                case 0xAC: repl = ""; break;
                case 0xAD: repl = "E"; break;
                case 0xAE: repl = "Yu"; break;
                case 0xAF: repl = "Ya"; break;
                case 0xB0: repl = "a"; break;
                case 0xB1: repl = "b"; break;
                case 0xB2: repl = "v"; break;
                case 0xB3: repl = "h"; break;
                case 0xB4: repl = "d"; break;
                case 0xB5: repl = "e"; break;
                case 0xB6: repl = "zh"; break;
                case 0xB7: repl = "z"; break;
                case 0xB8: repl = "y"; break;
                case 0xB9: repl = "i"; break;
                case 0xBA: repl = "k"; break;
                case 0xBB: repl = "l"; break;
                case 0xBC: repl = "m"; break;
                case 0xBD: repl = "n"; break;
                case 0xBE: repl = "o"; break;
                case 0xBF: repl = "p"; break;
            }
        }
        else if (b0 == 0xD1)
        {
            switch (b1)
            {
                case 0x80: repl = "r"; break;
                case 0x81: repl = "s"; break;
                case 0x82: repl = "t"; break;
                case 0x83: repl = "u"; break;
                case 0x84: repl = "f"; break;
                case 0x85: repl = "kh"; break;
                case 0x86: repl = "ts"; break;
                case 0x87: repl = "ch"; break;
                case 0x88: repl = "sh"; break;
                case 0x89: repl = "shch"; break;
                case 0x8A: repl = ""; break;
                case 0x8B: repl = "y"; break;
                case 0x8C: repl = ""; break;
                case 0x8D: repl = "e"; break;
                case 0x8E: repl = "yu"; break;
                case 0x8F: repl = "ya"; break;
                case 0x91: repl = "yo"; break;
                case 0x94: repl = "ye"; break;
                case 0x96: repl = "i"; break;
                case 0x97: repl = "yi"; break;
            }
        }

        if (repl.length())
        {
            out += repl;
        }
        else
        {
            out += ' ';
        }
        i += 2;
    }

    while (out.indexOf("  ") >= 0)
    {
        out.replace("  ", " ");
    }

    return trim_track_label(out);
}
String track_display_from_name(String value)
{
    int slash = value.lastIndexOf('/');
    if (slash >= 0)
    {
        value = value.substring(slash + 1);
    }
    int dot = value.lastIndexOf('.');
    if (dot > 0)
    {
        value = value.substring(0, dot);
    }
    value.replace('_', ' ');
    value.replace('-', ' ');
    return transliterate_track_label(value);
}

String track_key_from_name(String value)
{
    value = track_display_from_name(value);
    value.toLowerCase();
    String key;
    key.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i)
    {
        char c = value.charAt(i);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
        {
            key += c;
        }
    }
    return key;
}

bool is_supported_music_file(const String &name)
{
    String lowered = name;
    lowered.toLowerCase();
    return lowered.endsWith(".mp3") || lowered.endsWith(".wav");
}

String music_group_from_path(const String &path)
{
    String root = String(s_music_root);
    if (!root.endsWith("/"))
    {
        root += "/";
    }
    if (!path.startsWith(root))
    {
        return "Unknown";
    }
    String relative = path.substring(root.length());
    const int lastSlash = relative.lastIndexOf('/');
    if (lastSlash <= 0)
    {
        return "Songs";
    }

    // Album key is the real folder path that contains the track.
    // This supports unlimited nested albums under /songs.
    String group = relative.substring(0, lastSlash);
    group.replace("/", " / ");
    group.replace('_', ' ');
    group.replace('-', ' ');
    group = transliterate_track_label(group);
    return group.length() ? group : String("Unknown");
}

String music_folder_key_from_path(const String &path)
{
    String root = String(s_music_root);
    if (!root.endsWith("/"))
    {
        root += "/";
    }
    if (!path.startsWith(root))
    {
        return "";
    }
    String relative = path.substring(root.length());
    const int lastSlash = relative.lastIndexOf('/');
    if (lastSlash <= 0)
    {
        return "";
    }
    return relative.substring(0, lastSlash);
}

String folder_name_from_key(const String &folderKey)
{
    if (!folderKey.length())
    {
        return String("Songs");
    }
    String tail = folderKey;
    const int slash = tail.lastIndexOf('/');
    if (slash >= 0)
    {
        tail = tail.substring(slash + 1);
    }
    tail.replace('_', ' ');
    tail.replace('-', ' ');
    tail = transliterate_track_label(tail);
    return tail.length() ? tail : String("-");
}

void reset_music_library()
{
    s_music_track_count = 0;
    s_music_loaded = false;
    s_music_current_index = -1;
}

int compare_music_tracks(const void *lhs, const void *rhs)
{
    const MusicTrackEntry *a = static_cast<const MusicTrackEntry *>(lhs);
    const MusicTrackEntry *b = static_cast<const MusicTrackEntry *>(rhs);
    return strcmp(a->path, b->path);
}

int find_music_track_by_key(const String &key)
{
    if (!key.length())
    {
        return -1;
    }
    for (int i = 0; i < s_music_track_count; ++i)
    {
        if (track_key_from_name(s_music_tracks[i].path) == key)
        {
            return i;
        }
    }
    return -1;
}

void set_music_track_meta(int index, const String &title, const String &group)
{
    if (index < 0 || index >= s_music_track_count)
    {
        return;
    }
    String cleanTitle = trim_track_label(title);
    // Keep album grouping strictly folder-based; metadata group must not re-route tracks
    // to another album, otherwise album selection becomes inconsistent.
    String cleanGroup = trim_track_label(String(s_music_tracks[index].group));
    if (!cleanTitle.length())
    {
        cleanTitle = track_display_from_name(s_music_tracks[index].path);
    }
    if (!cleanGroup.length())
    {
        cleanGroup = trim_track_label(group);
    }
    if (!cleanGroup.length())
    {
        cleanGroup = "Unknown";
    }
    cleanTitle = transliterate_track_label(cleanTitle);
    cleanGroup = transliterate_track_label(cleanGroup);
    snprintf(s_music_tracks[index].title, sizeof(s_music_tracks[index].title), "%s", cleanTitle.c_str());
    snprintf(s_music_tracks[index].group, sizeof(s_music_tracks[index].group), "%s", cleanGroup.c_str());
}

void update_current_track_cache_from_index()
{
    if (s_music_current_index < 0 || s_music_current_index >= s_music_track_count)
    {
        snprintf(s_music_current_title, sizeof(s_music_current_title), "%s", "Nothing playing");
        snprintf(s_music_current_group, sizeof(s_music_current_group), "%s", "Ukrainian");
        s_music_current_path[0] = '\0';
        return;
    }

    snprintf(s_music_current_title, sizeof(s_music_current_title), "%s", s_music_tracks[s_music_current_index].title);
    snprintf(s_music_current_group, sizeof(s_music_current_group), "%s", s_music_tracks[s_music_current_index].group);
    snprintf(s_music_current_path, sizeof(s_music_current_path), "%s", s_music_tracks[s_music_current_index].path);
}

bool track_matches_selected_album(int index)
{
    if (index < 0 || index >= s_music_track_count)
    {
        return false;
    }
    if (s_music_selected_album < 0 || s_music_selected_album >= s_music_album_count)
    {
        return true;
    }
    const String selectedKey = String(s_music_selected_album_key);
    if (!selectedKey.length())
    {
        return true;
    }
    const String folder = music_folder_key_from_path(String(s_music_tracks[index].path));
    if (folder == selectedKey)
    {
        return true;
    }
    return folder.startsWith(selectedKey + "/");
}

int first_track_for_selected_album()
{
    for (int i = 0; i < s_music_track_count; ++i)
    {
        if (track_matches_selected_album(i))
        {
            return i;
        }
    }
    return -1;
}

int next_track_for_selected_album(int fromIndex, int delta)
{
    if (s_music_track_count <= 0)
    {
        return -1;
    }

    const int step = delta >= 0 ? 1 : -1;
    int idx = fromIndex;
    if (idx < 0 || idx >= s_music_track_count || !track_matches_selected_album(idx))
    {
        idx = first_track_for_selected_album();
    }
    if (idx < 0)
    {
        return -1;
    }

    for (int n = 0; n < s_music_track_count; ++n)
    {
        idx += step;
        if (idx < 0)
        {
            idx = s_music_track_count - 1;
        }
        if (idx >= s_music_track_count)
        {
            idx = 0;
        }
        if (track_matches_selected_album(idx))
        {
            return idx;
        }
    }
    return -1;
}

int active_track_count()
{
    if (s_music_track_count <= 0)
    {
        return 0;
    }
    if (s_music_selected_album < 0 || s_music_selected_album >= s_music_album_count)
    {
        return s_music_track_count;
    }

    int count = 0;
    for (int i = 0; i < s_music_track_count; ++i)
    {
        if (track_matches_selected_album(i))
        {
            ++count;
        }
    }
    return count;
}

int active_position_of_current()
{
    if (s_music_track_count <= 0)
    {
        return -1;
    }

    int pos = -1;
    int seen = 0;
    for (int i = 0; i < s_music_track_count; ++i)
    {
        if (!track_matches_selected_album(i))
        {
            continue;
        }
        if (i == s_music_current_index)
        {
            pos = seen;
        }
        ++seen;
    }
    return pos;
}

int track_index_at_active_position(int position)
{
    if (position < 0)
    {
        return -1;
    }

    int seen = 0;
    for (int i = 0; i < s_music_track_count; ++i)
    {
        if (!track_matches_selected_album(i))
        {
            continue;
        }
        if (seen == position)
        {
            return i;
        }
        ++seen;
    }
    return -1;
}

void rebuild_music_albums()
{
    s_music_album_count = 0;
    memset(s_music_album_has_children, 0, sizeof(s_music_album_has_children));

    const String browserRoot = String(s_music_browser_root);
    const String browserPrefix = browserRoot.length() ? (browserRoot + "/") : String("");

    for (int i = 0; i < s_music_track_count; ++i)
    {
        const String folder = music_folder_key_from_path(String(s_music_tracks[i].path));
        if (!folder.length())
        {
            continue;
        }

        String remainder;
        if (!browserRoot.length())
        {
            remainder = folder;
        }
        else if (folder == browserRoot)
        {
            continue;
        }
        else if (folder.startsWith(browserPrefix))
        {
            remainder = folder.substring(browserPrefix.length());
        }
        else
        {
            continue;
        }

        if (!remainder.length())
        {
            continue;
        }

        int slash = remainder.indexOf('/');
        bool hasChildren = slash >= 0;
        String child = hasChildren ? remainder.substring(0, slash) : remainder;
        String childKey = browserRoot.length() ? (browserRoot + "/" + child) : child;
        String childLabel = folder_name_from_key(childKey);

        bool exists = false;
        for (int j = 0; j < s_music_album_count; ++j)
        {
            if (strcmp(s_music_album_keys[j], childKey.c_str()) == 0)
            {
                exists = true;
                if (hasChildren)
                {
                    s_music_album_has_children[j] = true;
                }
                break;
            }
        }
        if (!exists && s_music_album_count < (int)kMaxMusicAlbums)
        {
            snprintf(s_music_album_keys[s_music_album_count], sizeof(s_music_album_keys[s_music_album_count]), "%s", childKey.c_str());
            snprintf(s_music_albums[s_music_album_count], sizeof(s_music_albums[s_music_album_count]), "%s", childLabel.c_str());
            s_music_album_has_children[s_music_album_count] = hasChildren;
            ++s_music_album_count;
        }
    }

    if (s_music_album_count > 1)
    {
        for (int i = 0; i < s_music_album_count - 1; ++i)
        {
            for (int j = i + 1; j < s_music_album_count; ++j)
            {
                if (strcmp(s_music_albums[i], s_music_albums[j]) > 0)
                {
                    char tmpName[48];
                    char tmpKey[64];
                    bool tmpChildren = s_music_album_has_children[i];
                    snprintf(tmpName, sizeof(tmpName), "%s", s_music_albums[i]);
                    snprintf(tmpKey, sizeof(tmpKey), "%s", s_music_album_keys[i]);
                    snprintf(s_music_albums[i], sizeof(s_music_albums[i]), "%s", s_music_albums[j]);
                    snprintf(s_music_album_keys[i], sizeof(s_music_album_keys[i]), "%s", s_music_album_keys[j]);
                    s_music_album_has_children[i] = s_music_album_has_children[j];
                    snprintf(s_music_albums[j], sizeof(s_music_albums[j]), "%s", tmpName);
                    snprintf(s_music_album_keys[j], sizeof(s_music_album_keys[j]), "%s", tmpKey);
                    s_music_album_has_children[j] = tmpChildren;
                }
            }
        }
    }
}

void set_selected_album_by_name(const char *name)
{
    s_music_selected_album = -1;
    s_music_selected_album_key[0] = '\0';
    if (name && name[0] && strcmp(name, "All") != 0)
    {
        for (int i = 0; i < s_music_album_count; ++i)
        {
            if (strcmp(s_music_albums[i], name) == 0)
            {
                s_music_selected_album = i;
                snprintf(s_music_selected_album_key, sizeof(s_music_selected_album_key), "%s", s_music_album_keys[i]);
                break;
            }
        }
    }

    if (s_music_selected_album >= 0 && s_music_selected_album < s_music_album_count)
    {
        snprintf(s_music_selected_album_name, sizeof(s_music_selected_album_name), "%s", s_music_albums[s_music_selected_album]);
    }
    else
    {
        snprintf(s_music_selected_album_name, sizeof(s_music_selected_album_name), "%s", "All");
    }
}

void set_selected_album_by_key(const char *key)
{
    s_music_selected_album = -1;
    s_music_selected_album_key[0] = '\0';
    if (key && key[0])
    {
        for (int i = 0; i < s_music_album_count; ++i)
        {
            if (strcmp(s_music_album_keys[i], key) == 0)
            {
                s_music_selected_album = i;
                snprintf(s_music_selected_album_key, sizeof(s_music_selected_album_key), "%s", s_music_album_keys[i]);
                break;
            }
        }
    }
    if (s_music_selected_album >= 0 && s_music_selected_album < s_music_album_count)
    {
        snprintf(s_music_selected_album_name, sizeof(s_music_selected_album_name), "%s", s_music_albums[s_music_selected_album]);
    }
    else
    {
        snprintf(s_music_selected_album_name, sizeof(s_music_selected_album_name), "%s", "All");
    }
}

void clamp_current_track_to_selected_album()
{
    if (s_music_track_count <= 0)
    {
        s_music_current_index = -1;
        update_current_track_cache_from_index();
        return;
    }

    if (s_music_selected_album >= 0 && !track_matches_selected_album(s_music_current_index))
    {
        s_music_current_index = first_track_for_selected_album();
    }

    if (s_music_current_index < 0 || s_music_current_index >= s_music_track_count)
    {
        s_music_current_index = first_track_for_selected_album();
    }

    update_current_track_cache_from_index();
}

void apply_titles_from_text_file(const char *txt_path)
{
    if (!txt_path || !txt_path[0] || s_music_track_count == 0)
    {
        return;
    }

    File file = SD.open(txt_path, FILE_READ);
    if (!file)
    {
        return;
    }

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line = trim_track_label(line);
        if (!line.length() || line.startsWith("#") || line.startsWith("//"))
        {
            continue;
        }

        int mappedIndex = -1;
        String parsedTitle;
        String parsedGroup;
        int sep = line.indexOf('|');
        if (sep < 0)
        {
            sep = line.indexOf(';');
        }
        if (sep < 0)
        {
            sep = line.indexOf('\t');
        }

        if (sep >= 0)
        {
            String first = trim_track_label(line.substring(0, sep));
            String rest = trim_track_label(line.substring(sep + 1));
            int sep2 = rest.indexOf('|');
            if (sep2 < 0)
            {
                sep2 = rest.indexOf(';');
            }
            if (sep2 < 0)
            {
                sep2 = rest.indexOf('\t');
            }
            if (sep2 >= 0)
            {
                parsedTitle = trim_track_label(rest.substring(0, sep2));
                parsedGroup = trim_track_label(rest.substring(sep2 + 1));
            }
            else
            {
                parsedTitle = rest;
            }

            bool firstIsIndex = first.length() > 0;
            for (size_t i = 0; i < first.length(); ++i)
            {
                char c = first.charAt(i);
                if (c < '0' || c > '9')
                {
                    firstIsIndex = false;
                    break;
                }
            }

            if (!firstIsIndex && (first.endsWith(".mp3") || first.endsWith(".wav") || first.indexOf('/') >= 0 || first.indexOf('_') >= 0))
            {
                mappedIndex = find_music_track_by_key(track_key_from_name(first));
            }
            else
            {
                parsedTitle = first;
            }
        }
        else
        {
            parsedTitle = line;
            while (parsedTitle.length() > 2 && parsedTitle.charAt(0) >= '0' && parsedTitle.charAt(0) <= '9')
            {
                parsedTitle.remove(0, 1);
            }
            parsedTitle = trim_track_label(parsedTitle);
            if (parsedTitle.startsWith(".") || parsedTitle.startsWith("-") || parsedTitle.startsWith(")"))
            {
                parsedTitle.remove(0, 1);
                parsedTitle = trim_track_label(parsedTitle);
            }
        }

        if (mappedIndex >= 0 && mappedIndex < s_music_track_count)
        {
            set_music_track_meta(mappedIndex, parsedTitle, parsedGroup);
            continue;
        }

        if (!parsedTitle.length())
        {
            continue;
        }
        // Do not apply unnamed sequential metadata lines to global library.
        // It causes cross-album title corruption when albums are added/reshuffled.
    }

    file.close();
}


void scan_music_tree(const String &folder, String &txtPath)
{
    File dir = SD.open(folder.c_str());
    if (!dir)
    {
        return;
    }

    while (true)
    {
        File f = dir.openNextFile();
        if (!f)
        {
            break;
        }

        String name = f.name();
        if (f.isDirectory())
        {
            if (name != "." && name != "..")
            {
                String childPath = name.startsWith("/") ? name : (folder + "/" + name);
                scan_music_tree(childPath, txtPath);
            }
            f.close();
            continue;
        }

        String lowered = name;
        lowered.toLowerCase();
        if (is_supported_music_file(lowered))
        {
            if (s_music_track_count < (int)kMaxMusicTracks)
            {
                String fullPath = name.startsWith("/") ? name : (folder + "/" + name);
                String title = track_display_from_name(fullPath);
                String group = music_group_from_path(fullPath);
                snprintf(s_music_tracks[s_music_track_count].path, sizeof(s_music_tracks[s_music_track_count].path), "%s", fullPath.c_str());
                snprintf(s_music_tracks[s_music_track_count].title, sizeof(s_music_tracks[s_music_track_count].title), "%s", title.c_str());
                snprintf(s_music_tracks[s_music_track_count].group, sizeof(s_music_tracks[s_music_track_count].group), "%s", group.c_str());
                s_music_track_count++;
            }
        }
        else if (!txtPath.length() && lowered.endsWith(".txt"))
        {
            txtPath = name.startsWith("/") ? name : (folder + "/" + name);
        }

        f.close();
    }

    dir.close();
}
bool load_music_library()
{
    if (!s_sd_ready)
    {
        return false;
    }

    char restoreAlbum[48];
    char restoreAlbumKey[64];
    char restoreBrowserRoot[64];
    snprintf(restoreAlbum, sizeof(restoreAlbum), "%s", s_music_selected_album_name);
    snprintf(restoreAlbumKey, sizeof(restoreAlbumKey), "%s", s_music_selected_album_key);
    snprintf(restoreBrowserRoot, sizeof(restoreBrowserRoot), "%s", s_music_browser_root);

    bool rootFound = false;
    String txtPath;

    for (size_t rootIndex = 0; rootIndex < kMusicRootCount; ++rootIndex)
    {
        reset_music_library();
        txtPath = "";
        snprintf(s_music_root, sizeof(s_music_root), "%s", kMusicRoots[rootIndex]);

        File dir = SD.open(s_music_root);
        if (!dir)
        {
            continue;
        }

        rootFound = true;
        dir.close();
        scan_music_tree(String(s_music_root), txtPath);
        if (s_music_track_count > 0)
        {
            break;
        }
    }

    if (!rootFound)
    {
        set_status("No songs folder");
        return false;
    }

    if (s_music_track_count <= 0)
    {
        set_status("No songs under %s", s_music_root);
        return false;
    }

    qsort(s_music_tracks, (size_t)s_music_track_count, sizeof(MusicTrackEntry), compare_music_tracks);
    // NOTE: keep track titles/path mapping deterministic by file names only.
    // TXT import can mix titles between albums when libraries are re-ordered.
    (void)txtPath;

    s_music_browser_root[0] = '\0';
    rebuild_music_albums();
    if (restoreBrowserRoot[0])
    {
        snprintf(s_music_browser_root, sizeof(s_music_browser_root), "%s", restoreBrowserRoot);
        rebuild_music_albums();
    }
    set_selected_album_by_key(restoreAlbumKey);
    if ((s_music_selected_album < 0 || s_music_selected_album >= s_music_album_count) && restoreAlbum[0])
    {
        set_selected_album_by_name(restoreAlbum);
    }
    if ((s_music_selected_album < 0 || s_music_selected_album >= s_music_album_count) && s_music_album_count > 0)
    {
        set_selected_album_by_key(s_music_album_keys[0]);
    }

    s_music_loaded = true;
    clamp_current_track_to_selected_album();
    set_status("Library tracks: %d", s_music_track_count);
    return true;
}
void set_status(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_status, sizeof(s_status), fmt, args);
    va_end(args);
    Serial.printf("[sd] %s\n", s_status);
}

bool choose_random_file(const char *folder, String &out_path)
{
    File dir = SD.open(folder);
    if (!dir)
    {
        return false;
    }

    int count = 0;
    while (true)
    {
        File f = dir.openNextFile();
        if (!f)
        {
            break;
        }
        if (!f.isDirectory())
        {
            String name = f.name();
            name.toLowerCase();
            if (name.endsWith(".wav") || name.endsWith(".mp3"))
            {
                count++;
            }
        }
        f.close();
    }
    dir.close();

    if (count == 0)
    {
        return false;
    }

    const int target = random(count);
    dir = SD.open(folder);
    if (!dir)
    {
        return false;
    }

    int idx = 0;
    while (true)
    {
        File f = dir.openNextFile();
        if (!f)
        {
            break;
        }
        if (!f.isDirectory())
        {
            String name = f.name();
            String lowered = name;
            lowered.toLowerCase();
            if (lowered.endsWith(".wav") || lowered.endsWith(".mp3"))
            {
                if (idx == target)
                {
                    out_path = String(folder) + "/" + name;
                    f.close();
                    dir.close();
                    return true;
                }
                idx++;
            }
        }
        f.close();
    }
    dir.close();
    return false;
}

bool filename_matches_any(const String &name, const char *const *patterns, size_t pattern_count)
{
    if (!patterns || pattern_count == 0)
    {
        return true;
    }

    for (size_t i = 0; i < pattern_count; ++i)
    {
        if (name.indexOf(patterns[i]) >= 0)
        {
            return true;
        }
    }
    return false;
}

bool choose_random_file_filtered(const char *folder, const char *const *patterns, size_t pattern_count, String &out_path)
{
    File dir = SD.open(folder);
    if (!dir)
    {
        return false;
    }

    int count = 0;
    while (true)
    {
        File f = dir.openNextFile();
        if (!f)
        {
            break;
        }
        if (!f.isDirectory())
        {
            String name = f.name();
            name.toLowerCase();
            if ((name.endsWith(".wav") || name.endsWith(".mp3")) &&
                filename_matches_any(name, patterns, pattern_count))
            {
                count++;
            }
        }
        f.close();
    }
    dir.close();

    if (count == 0)
    {
        return false;
    }

    const int target = random(count);
    dir = SD.open(folder);
    if (!dir)
    {
        return false;
    }

    int idx = 0;
    while (true)
    {
        File f = dir.openNextFile();
        if (!f)
        {
            break;
        }
        if (!f.isDirectory())
        {
            String name = f.name();
            String lowered = name;
            lowered.toLowerCase();
            if ((lowered.endsWith(".wav") || lowered.endsWith(".mp3")) &&
                filename_matches_any(lowered, patterns, pattern_count))
            {
                if (idx == target)
                {
                    out_path = String(folder) + "/" + name;
                    f.close();
                    dir.close();
                    return true;
                }
                idx++;
            }
        }
        f.close();
    }
    dir.close();
    return false;
}

bool play_random_from_folders(const char *const *folders, size_t folder_count)
{
    if (!s_sd_ready || !folders || folder_count == 0)
    {
        return false;
    }

    for (size_t i = 0; i < folder_count; ++i)
    {
        if (!folders[i] || !folders[i][0])
        {
            continue;
        }

        String path;
        if (!choose_random_file(folders[i], path))
        {
            continue;
        }

        const bool ok = radio_vendor_play_sd(path.c_str());
        set_status(ok ? path.c_str() : "SD play failed");
        return ok;
    }

    set_status("No media for reaction");
    return false;
}
} // namespace

bool sd_vendor_init()
{
    SPI.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);
    s_sd_ready = SD.begin(kSdCs, SPI);
    if (s_sd_ready)
    {
        uint64_t total = SD.totalBytes();
        uint64_t used = SD.usedBytes();
        set_status("SD ready %lluMB free", (unsigned long long)((total - used) / (1024ULL * 1024ULL)));
    }
    else
    {
        set_status("SD mount failed");
    }
    return s_sd_ready;
}

bool sd_vendor_ensure_ready()
{
    if (s_sd_ready)
    {
        return true;
    }
    return sd_vendor_init();
}

bool sd_vendor_ready()
{
    return s_sd_ready;
}

const char *sd_vendor_status()
{
    return s_status;
}

bool sd_vendor_is_busy()
{
    return radio_vendor_is_playing() && radio_vendor_current_index() < 0;
}

static bool play_filtered_from_folder(const char *folder, const char *const *patterns, size_t pattern_count, const char *missing_label)
{
    String path;
    if (!s_sd_ready || !choose_random_file_filtered(folder, patterns, pattern_count, path))
    {
        set_status(missing_label);
        return false;
    }

    const bool ok = radio_vendor_play_sd(path.c_str());
    set_status(ok ? path.c_str() : "SD play failed");
    return ok;
}

bool sd_vendor_play_random(const char *folder)
{
    if (!folder || !s_sd_ready)
    {
        return false;
    }

    String path;
    if (!choose_random_file(folder, path))
    {
        set_status("No media in %s", folder);
        return false;
    }

    const bool ok = radio_vendor_play_sd(path.c_str());
    set_status(ok ? path.c_str() : "SD play failed");
    return ok;
}

bool sd_vendor_play_murmur_mode(SdVoiceMode mode)
{
    static const char *kDay[] = {"murmur_hums_", "murmur_soft_", "murmur_"};
    static const char *kEvening[] = {"murmur_soft_", "murmur_"};
    static const char *kNight[] = {"murmur_night_", "murmur_soft_", "murmur_"};
    switch (mode)
    {
        case SD_VOICE_DAY: return play_filtered_from_folder("/ambient/murmur", kDay, sizeof(kDay) / sizeof(kDay[0]), "No day murmur");
        case SD_VOICE_EVENING: return play_filtered_from_folder("/ambient/murmur", kEvening, sizeof(kEvening) / sizeof(kEvening[0]), "No evening murmur");
        case SD_VOICE_NIGHT: return play_filtered_from_folder("/ambient/murmur", kNight, sizeof(kNight) / sizeof(kNight[0]), "No night murmur");
        default: return false;
    }
}

bool sd_vendor_play_selftalk_mode(SdVoiceMode mode)
{
    static const char *kDay[] = {"selftalk_playful_", "selftalk_reflective_", "selftalk_"};
    static const char *kEvening[] = {"selftalk_reflective_", "selftalk_soft_", "selftalk_"};
    static const char *kNight[] = {"selftalk_soft_", "selftalk_reflective_", "selftalk_"};
    switch (mode)
    {
        case SD_VOICE_DAY: return play_filtered_from_folder("/ambient/selftalk", kDay, sizeof(kDay) / sizeof(kDay[0]), "No day selftalk");
        case SD_VOICE_EVENING: return play_filtered_from_folder("/ambient/selftalk", kEvening, sizeof(kEvening) / sizeof(kEvening[0]), "No evening selftalk");
        case SD_VOICE_NIGHT: return play_filtered_from_folder("/ambient/selftalk", kNight, sizeof(kNight) / sizeof(kNight[0]), "No night selftalk");
        default: return false;
    }
}

bool sd_vendor_play_question_mode(SdVoiceMode mode)
{
    static const char *kDay[] = {"questions_curious_", "questions_checkin_", "questions_"};
    static const char *kEvening[] = {"questions_gentle_", "questions_checkin_", "questions_"};
    static const char *kNight[] = {"questions_gentle_", "questions_"};
    switch (mode)
    {
        case SD_VOICE_DAY: return play_filtered_from_folder("/ambient/questions", kDay, sizeof(kDay) / sizeof(kDay[0]), "No day question");
        case SD_VOICE_EVENING: return play_filtered_from_folder("/ambient/questions", kEvening, sizeof(kEvening) / sizeof(kEvening[0]), "No evening question");
        case SD_VOICE_NIGHT: return play_filtered_from_folder("/ambient/questions", kNight, sizeof(kNight) / sizeof(kNight[0]), "No night question");
        default: return false;
    }
}

bool sd_vendor_play_heard_mode(SdVoiceMode mode)
{
    static const char *kDay[] = {"heard_neutral_", "heard_"};
    static const char *kEvening[] = {"heard_warm_", "heard_"};
    static const char *kNight[] = {"heard_robotic_", "heard_"};
    switch (mode)
    {
        case SD_VOICE_DAY: return play_filtered_from_folder("/react/heard", kDay, sizeof(kDay) / sizeof(kDay[0]), "No day heard");
        case SD_VOICE_EVENING: return play_filtered_from_folder("/react/heard", kEvening, sizeof(kEvening) / sizeof(kEvening[0]), "No evening heard");
        case SD_VOICE_NIGHT: return play_filtered_from_folder("/react/heard", kNight, sizeof(kNight) / sizeof(kNight[0]), "No night heard");
        default: return false;
    }
}

bool sd_vendor_play_ack_mode(SdVoiceMode mode)
{
    static const char *kDay[] = {"ack_neutral_", "ack_"};
    static const char *kEvening[] = {"ack_warm_", "ack_"};
    static const char *kNight[] = {"ack_robotic_", "ack_"};
    switch (mode)
    {
        case SD_VOICE_DAY: return play_filtered_from_folder("/react/ack", kDay, sizeof(kDay) / sizeof(kDay[0]), "No day ack");
        case SD_VOICE_EVENING: return play_filtered_from_folder("/react/ack", kEvening, sizeof(kEvening) / sizeof(kEvening[0]), "No evening ack");
        case SD_VOICE_NIGHT: return play_filtered_from_folder("/react/ack", kNight, sizeof(kNight) / sizeof(kNight[0]), "No night ack");
        default: return false;
    }
}

bool sd_vendor_play_listening_mode(SdVoiceMode mode)
{
    static const char *kDay[] = {"listening_neutral_", "listening_"};
    static const char *kEvening[] = {"listening_gentle_", "listening_"};
    static const char *kNight[] = {"listening_robotic_", "listening_"};
    switch (mode)
    {
        case SD_VOICE_DAY: return play_filtered_from_folder("/react/listening", kDay, sizeof(kDay) / sizeof(kDay[0]), "No day listening");
        case SD_VOICE_EVENING: return play_filtered_from_folder("/react/listening", kEvening, sizeof(kEvening) / sizeof(kEvening[0]), "No evening listening");
        case SD_VOICE_NIGHT: return play_filtered_from_folder("/react/listening", kNight, sizeof(kNight) / sizeof(kNight[0]), "No night listening");
        default: return false;
    }
}

bool sd_vendor_play_curious_mode(SdVoiceMode mode)
{
    static const char *kDay[] = {"curious_neutral_", "curious_"};
    static const char *kEvening[] = {"curious_warm_", "curious_"};
    static const char *kNight[] = {"curious_robotic_", "curious_"};
    switch (mode)
    {
        case SD_VOICE_DAY: return play_filtered_from_folder("/react/curious", kDay, sizeof(kDay) / sizeof(kDay[0]), "No day curious");
        case SD_VOICE_EVENING: return play_filtered_from_folder("/react/curious", kEvening, sizeof(kEvening) / sizeof(kEvening[0]), "No evening curious");
        case SD_VOICE_NIGHT: return play_filtered_from_folder("/react/curious", kNight, sizeof(kNight) / sizeof(kNight[0]), "No night curious");
        default: return false;
    }
}

bool sd_vendor_play_murmur()
{
    return sd_vendor_play_murmur_mode(SD_VOICE_DAY);
}

bool sd_vendor_play_selftalk()
{
    return sd_vendor_play_selftalk_mode(SD_VOICE_DAY);
}

bool sd_vendor_play_question()
{
    return sd_vendor_play_question_mode(SD_VOICE_DAY);
}

bool sd_vendor_play_heard()
{
    return sd_vendor_play_heard_mode(SD_VOICE_DAY);
}

bool sd_vendor_play_ack()
{
    return sd_vendor_play_ack_mode(SD_VOICE_DAY);
}

bool sd_vendor_play_curious()
{
    return sd_vendor_play_curious_mode(SD_VOICE_DAY);
}

bool sd_vendor_music_refresh()
{
    return load_music_library();
}

bool sd_vendor_music_loaded()
{
    return s_music_loaded && s_music_track_count > 0;
}

bool sd_vendor_music_play_index(int index)
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return false;
    }
    if (index < 0 || index >= s_music_track_count)
    {
        return false;
    }
    if (!track_matches_selected_album(index))
    {
        set_status("Track outside album");
        return false;
    }

    const MusicTrackEntry &track = s_music_tracks[index];
    const bool ok = radio_vendor_play_sd_with_info(track.path, track.title, track.group);
    if (!ok)
    {
        set_status("SD play failed");
        return false;
    }

    s_music_current_index = index;
    update_current_track_cache_from_index();
    set_status("%s", track.title);
    return true;
}

bool sd_vendor_music_play_active_position(int position)
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return false;
    }

    const int count = active_track_count();
    if (position < 0 || position >= count)
    {
        return false;
    }

    const int trackIndex = track_index_at_active_position(position);
    if (trackIndex < 0)
    {
        return false;
    }

    return sd_vendor_music_play_index(trackIndex);
}

bool sd_vendor_music_play_current()
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return false;
    }
    clamp_current_track_to_selected_album();
    if (s_music_current_index < 0)
    {
        set_status("Album empty");
        return false;
    }
    return sd_vendor_music_play_index(s_music_current_index);
}

bool sd_vendor_music_play_next()
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return false;
    }
    clamp_current_track_to_selected_album();
    const int nextIndex = next_track_for_selected_album(s_music_current_index, +1);
    if (nextIndex < 0)
    {
        set_status("Album empty");
        return false;
    }
    return sd_vendor_music_play_index(nextIndex);
}

bool sd_vendor_music_play_previous()
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return false;
    }
    clamp_current_track_to_selected_album();
    const int prevIndex = next_track_for_selected_album(s_music_current_index, -1);
    if (prevIndex < 0)
    {
        set_status("Album empty");
        return false;
    }
    return sd_vendor_music_play_index(prevIndex);
}

bool sd_vendor_music_play_random_current_album()
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return false;
    }
    const int activeCount = active_track_count();
    if (activeCount <= 0)
    {
        set_status("Album empty");
        return false;
    }

    int targetPos = random(activeCount);
    int targetTrack = track_index_at_active_position(targetPos);
    if (activeCount > 1 && targetTrack == s_music_current_index)
    {
        targetPos = (targetPos + 1) % activeCount;
        targetTrack = track_index_at_active_position(targetPos);
    }
    if (targetTrack < 0)
    {
        return false;
    }
    return sd_vendor_music_play_index(targetTrack);
}

int sd_vendor_music_count()
{
    if (!s_music_loaded && s_sd_ready)
    {
        load_music_library();
    }
    return s_music_track_count;
}

int sd_vendor_music_index()
{
    if (!s_music_loaded && s_sd_ready)
    {
        load_music_library();
    }
    clamp_current_track_to_selected_album();
    return s_music_current_index;
}

const char *sd_vendor_music_title()
{
    if (!s_music_loaded && s_sd_ready)
    {
        load_music_library();
    }
    clamp_current_track_to_selected_album();
    return s_music_current_title;
}


const char *sd_vendor_music_title_at(int index)
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return "";
    }
    if (index < 0 || index >= s_music_track_count)
    {
        return "";
    }
    return s_music_tracks[index].title;
}
const char *sd_vendor_music_group()
{
    if (!s_music_loaded && s_sd_ready)
    {
        load_music_library();
    }
    clamp_current_track_to_selected_album();
    return s_music_current_group;
}

const char *sd_vendor_music_path()
{
    if (!s_music_loaded && s_sd_ready)
    {
        load_music_library();
    }
    clamp_current_track_to_selected_album();
    return s_music_current_path;
}

int sd_vendor_music_album_count()
{
    if ((!s_music_loaded || s_music_track_count <= 0) && s_sd_ready)
    {
        load_music_library();
    }
    return s_music_album_count;
}

int sd_vendor_music_album_index()
{
    if ((!s_music_loaded || s_music_track_count <= 0) && s_sd_ready)
    {
        load_music_library();
    }
    if (s_music_selected_album < 0 || s_music_selected_album >= s_music_album_count)
    {
        return 0;
    }
    return s_music_selected_album;
}

const char *sd_vendor_music_album_name()
{
    if ((!s_music_loaded || s_music_track_count <= 0) && s_sd_ready)
    {
        load_music_library();
    }
    return s_music_selected_album_name;
}

const char *sd_vendor_music_album_name_at(int index)
{
    if ((!s_music_loaded || s_music_track_count <= 0) && s_sd_ready)
    {
        load_music_library();
    }
    if (index < 0 || index >= s_music_album_count)
    {
        return "";
    }
    return s_music_albums[index];
}

bool sd_vendor_music_select_album(int index)
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return false;
    }

    if (index < 0 || index >= s_music_album_count)
    {
        return false;
    }

    s_music_selected_album = index;
    snprintf(s_music_selected_album_name, sizeof(s_music_selected_album_name), "%s", s_music_albums[index]);
    snprintf(s_music_selected_album_key, sizeof(s_music_selected_album_key), "%s", s_music_album_keys[index]);
    clamp_current_track_to_selected_album();
    set_status("Album: %s", s_music_selected_album_name);
    return true;
}

bool sd_vendor_music_select_album_next(int delta)
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return false;
    }

    const int total = s_music_album_count;
    if (total <= 0)
    {
        return false;
    }

    const int step = (delta < 0) ? -1 : 1;
    int next = sd_vendor_music_album_index() + step;
    if (next < 0)
    {
        next = total - 1;
    }
    if (next >= total)
    {
        next = 0;
    }
    return sd_vendor_music_select_album(next);
}

bool sd_vendor_music_album_has_children(int index)
{
    if ((!s_music_loaded || s_music_track_count <= 0) && s_sd_ready)
    {
        load_music_library();
    }
    if (index < 0 || index >= s_music_album_count)
    {
        return false;
    }
    return s_music_album_has_children[index];
}

bool sd_vendor_music_enter_selected_album()
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return false;
    }
    if (s_music_selected_album < 0 || s_music_selected_album >= s_music_album_count)
    {
        return false;
    }
    if (!s_music_album_has_children[s_music_selected_album])
    {
        return false;
    }

    snprintf(s_music_browser_root, sizeof(s_music_browser_root), "%s", s_music_album_keys[s_music_selected_album]);
    rebuild_music_albums();
    if (s_music_album_count > 0)
    {
        set_selected_album_by_key(s_music_album_keys[0]);
    }
    else
    {
        s_music_selected_album = -1;
        s_music_selected_album_key[0] = '\0';
        snprintf(s_music_selected_album_name, sizeof(s_music_selected_album_name), "%s", "All");
    }
    set_status("Folder: %s", s_music_browser_root);
    return true;
}

bool sd_vendor_music_browser_go_root()
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return false;
    }
    s_music_browser_root[0] = '\0';
    rebuild_music_albums();
    if (s_music_album_count > 0)
    {
        set_selected_album_by_key(s_music_album_keys[0]);
    }
    else
    {
        s_music_selected_album = -1;
        s_music_selected_album_key[0] = '\0';
        snprintf(s_music_selected_album_name, sizeof(s_music_selected_album_name), "%s", "All");
    }
    return true;
}

int sd_vendor_music_count_active()
{
    if ((!s_music_loaded || s_music_track_count <= 0) && s_sd_ready)
    {
        load_music_library();
    }
    return active_track_count();
}

int sd_vendor_music_position_active()
{
    if ((!s_music_loaded || s_music_track_count <= 0) && s_sd_ready)
    {
        load_music_library();
    }
    clamp_current_track_to_selected_album();
    return active_position_of_current();
}

const char *sd_vendor_music_title_at_active_offset(int offset)
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return "";
    }

    const int count = active_track_count();
    if (count <= 0)
    {
        return "";
    }

    clamp_current_track_to_selected_album();
    int pos = active_position_of_current();
    if (pos < 0)
    {
        pos = 0;
    }

    int wrapped = (pos + offset) % count;
    if (wrapped < 0)
    {
        wrapped += count;
    }
    const int trackIndex = track_index_at_active_position(wrapped);
    if (trackIndex < 0 || trackIndex >= s_music_track_count)
    {
        return "";
    }
    return s_music_tracks[trackIndex].title;
}

const char *sd_vendor_music_title_at_active(int position)
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return "";
    }

    const int count = active_track_count();
    if (position < 0 || position >= count)
    {
        return "";
    }

    const int trackIndex = track_index_at_active_position(position);
    if (trackIndex < 0 || trackIndex >= s_music_track_count)
    {
        return "";
    }

    return s_music_tracks[trackIndex].title;
}

const char *sd_vendor_music_group_at_active(int position)
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return "";
    }

    const int count = active_track_count();
    if (position < 0 || position >= count)
    {
        return "";
    }

    const int trackIndex = track_index_at_active_position(position);
    if (trackIndex < 0 || trackIndex >= s_music_track_count)
    {
        return "";
    }

    return s_music_tracks[trackIndex].group;
}

const char *sd_vendor_music_path_at_active(int position)
{
    if ((!s_music_loaded || s_music_track_count <= 0) && !load_music_library())
    {
        return "";
    }

    const int count = active_track_count();
    if (position < 0 || position >= count)
    {
        return "";
    }

    const int trackIndex = track_index_at_active_position(position);
    if (trackIndex < 0 || trackIndex >= s_music_track_count)
    {
        return "";
    }

    return s_music_tracks[trackIndex].path;
}


