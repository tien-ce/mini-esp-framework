#include "core/web/web_auth.h"
#include "core/web_server_task.h"

bool web_authenticate(AsyncWebServerRequest *request) {
    if (request == nullptr) {
        return false;
    }
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        request->requestAuthentication();
        return false;
    }
    return true;
}
