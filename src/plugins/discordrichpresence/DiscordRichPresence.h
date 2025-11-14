bool init_discord();
void update_presence(const char* track, const char* artist, const char* album, const char* cover_url, int duration_seconds);
char* upload_cover_image(const char* file_path);
void keep_connection_alive();
void sleep(int milliseconds);