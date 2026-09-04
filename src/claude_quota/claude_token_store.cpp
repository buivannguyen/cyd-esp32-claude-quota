#include "claude_token_store.h"
#include <Preferences.h>

static const char *NVS_NAMESPACE = "claude";
static const char *NVS_KEY_TOKEN = "token";

bool claude_token_has()
{
    Preferences p;
    p.begin(NVS_NAMESPACE, true); // RO
    bool has = p.isKey(NVS_KEY_TOKEN) && p.getString(NVS_KEY_TOKEN, "").length() > 0;
    p.end();
    return has;
}

String claude_token_load()
{
    Preferences p;
    p.begin(NVS_NAMESPACE, false); // RW so the namespace gets created on first run
    String token = p.isKey(NVS_KEY_TOKEN) ? p.getString(NVS_KEY_TOKEN, "") : String();
    p.end();
    return token;
}

void claude_token_save(const String &token)
{
    Preferences p;
    p.begin(NVS_NAMESPACE, false);
    p.putString(NVS_KEY_TOKEN, token);
    p.end();
}
