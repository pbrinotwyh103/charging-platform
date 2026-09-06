TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    user_client \
    admin_client \
    server_app \
    protocol_tests \
    phase1_tests \
    database_repository_tests \
    service_tests \
    business_integration_tests \
    user_ui_tests \
    admin_controller_tests \
    admin_ui_tests

user_client.file = user-client/user-client.pro
admin_client.file = admin-client/admin-client.pro
server_app.file = server/server.pro
protocol_tests.file = tests/protocol-tests.pro
phase1_tests.file = tests/phase1-tests.pro
database_repository_tests.file = tests/database-repository-tests.pro
service_tests.file = tests/service-tests.pro
business_integration_tests.file = tests/business-integration-tests.pro
user_ui_tests.file = tests/user-ui-tests.pro
admin_controller_tests.file = tests/admin-controller-tests.pro
admin_ui_tests.file = tests/admin-ui-tests.pro

admin_client.depends = user_client
server_app.depends = admin_client
protocol_tests.depends = server_app
phase1_tests.depends = protocol_tests
database_repository_tests.depends = phase1_tests
service_tests.depends = database_repository_tests
business_integration_tests.depends = service_tests
user_ui_tests.depends = business_integration_tests
admin_controller_tests.depends = user_ui_tests
admin_ui_tests.depends = admin_controller_tests
