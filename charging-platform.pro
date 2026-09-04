TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    user_client \
    admin_client \
    server_app \
    protocol_tests

user_client.file = user-client/user-client.pro
admin_client.file = admin-client/admin-client.pro
server_app.file = server/server.pro
protocol_tests.file = tests/protocol-tests.pro

admin_client.depends = user_client
server_app.depends = admin_client
protocol_tests.depends = server_app
