#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "discord_game_sdk.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <string.h>
#endif

#define DISCORD_REQUIRE(x) assert(x == DiscordResult_Ok)

struct Application {
    struct IDiscordCore* core;
    struct IDiscordUserManager* users;
    struct IDiscordAchievementManager* achievements;
    struct IDiscordActivityManager* activities;
    struct IDiscordRelationshipManager* relationships;
    struct IDiscordApplicationManager* application;
    struct IDiscordLobbyManager* lobbies;
    DiscordUserId user_id;
};

void DISCORD_CALLBACK UpdateActivityCallback(void* data, enum EDiscordResult result)
{
    DISCORD_REQUIRE(result);
}

bool DISCORD_CALLBACK RelationshipPassFilter(void* data, struct DiscordRelationship* relationship)
{
    return (relationship->type == DiscordRelationshipType_Friend);
}

bool DISCORD_CALLBACK RelationshipSnowflakeFilter(void* data,
                                                 struct DiscordRelationship* relationship)
{
    struct Application* app = (struct Application*)data;

    return (relationship->type == DiscordRelationshipType_Friend &&
            relationship->user.id < app->user_id);
}

void DISCORD_CALLBACK OnRelationshipsRefresh(void* data)
{
    struct Application* app = (struct Application*)data;
    struct IDiscordRelationshipManager* module = app->relationships;

    module->filter(module, app, RelationshipPassFilter);

    int32_t unfiltered_count = 0;
    DISCORD_REQUIRE(module->count(module, &unfiltered_count));

    module->filter(module, app, RelationshipSnowflakeFilter);

    int32_t filtered_count = 0;
    DISCORD_REQUIRE(module->count(module, &filtered_count));

    printf("=== Cool Friends ===\n");
    for (int32_t i = 0; i < filtered_count; i += 1) {
        struct DiscordRelationship relationship;
        DISCORD_REQUIRE(module->get_at(module, i, &relationship));

        printf("%lld %s#%s\n",
               relationship.user.id,
               relationship.user.username,
               relationship.user.discriminator);
    }
    printf("(%d friends less cool than you omitted)\n", unfiltered_count - filtered_count);

    struct DiscordActivity activity;
    memset(&activity, 0, sizeof(activity));
    sprintf(activity.details, "Cooler than %d friends", unfiltered_count - filtered_count);
    sprintf(activity.state, "%d friends total", unfiltered_count);

    app->activities->update_activity(app->activities, &activity, app, UpdateActivityCallback);
}

void DISCORD_CALLBACK OnUserUpdated(void* data)
{
    struct Application* app = (struct Application*)data;
    struct DiscordUser user;
    app->users->get_current_user(app->users, &user);
    app->user_id = user.id;
}

void DISCORD_CALLBACK OnOAuth2Token(void* data,
                                    enum EDiscordResult result,
                                    struct DiscordOAuth2Token* token)
{
    if (result == DiscordResult_Ok) {
        printf("OAuth2 token: %s\n", token->access_token);
    }
    else {
        printf("GetOAuth2Token failed with %d\n", (int)result);
    }
}

void DISCORD_CALLBACK OnLobbyConnect(void* data,
                                     enum EDiscordResult result,
                                     struct DiscordLobby* lobby)
{
    printf("LobbyConnect returned %d\n", (int)result);
}

int main(int argc, char** argv)
{
    struct Application app;
    memset(&app, 0, sizeof(app));

    struct IDiscordUserEvents users_events;
    memset(&users_events, 0, sizeof(users_events));
    users_events.on_current_user_update = OnUserUpdated;

    struct IDiscordActivityEvents activities_events;
    memset(&activities_events, 0, sizeof(activities_events));

    struct IDiscordRelationshipEvents relationships_events;
    memset(&relationships_events, 0, sizeof(relationships_events));
    relationships_events.on_refresh = OnRelationshipsRefresh;

    struct DiscordCreateParams params;
    DiscordCreateParamsSetDefault(&params);
    params.client_id = 418559331265675294;
    params.flags = DiscordCreateFlags_Default;
    params.event_data = &app;
    params.activity_events = &activities_events;
    params.relationship_events = &relationships_events;
    params.user_events = &users_events;
    DISCORD_REQUIRE(DiscordCreate(DISCORD_VERSION, &params, &app.core));

    app.users = app.core->get_user_manager(app.core);
    app.achievements = app.core->get_achievement_manager(app.core);
    app.activities = app.core->get_activity_manager(app.core);
    app.application = app.core->get_application_manager(app.core);
    app.lobbies = app.core->get_lobby_manager(app.core);

    app.lobbies->connect_lobby_with_activity_secret(
      app.lobbies, "invalid_secret", &app, OnLobbyConnect);

    app.application->get_oauth2_token(app.application, &app, OnOAuth2Token);

    DiscordBranch branch;
    app.application->get_current_branch(app.application, &branch);
    printf("Current branch %s\n", branch);

    app.relationships = app.core->get_relationship_manager(app.core);

    for (;;) {
        DISCORD_REQUIRE(app.core->run_callbacks(app.core));

#ifdef _WIN32
        Sleep(16);
#else
        usleep(16 * 1000);
#endif
    }

    return 0;
}
