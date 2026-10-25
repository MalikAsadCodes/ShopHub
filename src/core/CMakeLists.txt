# shophub_core: all business logic and data access.
# It deliberately does NOT link Qt Widgets/Gui, so it can be tested without a GUI.
add_library(shophub_core STATIC
    common/Money.h               common/Money.cpp
    common/Result.h
    common/Validation.h          common/Validation.cpp
    common/Logger.h              common/Logger.cpp

    domain/Types.h               domain/Types.cpp
    domain/OrderStatus.h         domain/OrderStatus.cpp
    domain/Session.h
    domain/PriceTier.h
    domain/Qist.h

    database/Database.h          database/Database.cpp
    database/Transaction.h       database/Transaction.cpp
    database/Migrations.h        database/Migrations.cpp
    database/SqlUtil.h           database/SqlUtil.cpp

    repositories/RepositoryBase.h
    repositories/Repositories.h
    repositories/UserRepository.h       repositories/UserRepository.cpp
    repositories/CategoryRepository.h   repositories/CategoryRepository.cpp
    repositories/ProductRepository.h    repositories/ProductRepository.cpp
    repositories/CartRepository.h       repositories/CartRepository.cpp
    repositories/OrderRepository.h      repositories/OrderRepository.cpp
    repositories/MiscRepositories.h     repositories/MiscRepositories.cpp
    repositories/KhataRepository.h      repositories/KhataRepository.cpp
    repositories/QistRepository.h       repositories/QistRepository.cpp

    security/PasswordHasher.h    security/PasswordHasher.cpp

    services/Access.h
    services/ImageStore.h        services/ImageStore.cpp
    services/AuthService.h       services/AuthService.cpp
    services/CatalogService.h    services/CatalogService.cpp
    services/PricingService.h    services/PricingService.cpp
    services/CartService.h       services/CartService.cpp
    services/CheckoutService.h   services/CheckoutService.cpp
    services/OrderService.h      services/OrderService.cpp
    services/InventoryService.h  services/InventoryService.cpp
    services/KhataService.h      services/KhataService.cpp
    services/QistService.h       services/QistService.cpp
    services/DashboardService.h  services/DashboardService.cpp
    services/AppContext.h        services/AppContext.cpp
    services/DemoSeeder.h        services/DemoSeeder.cpp
)

target_include_directories(shophub_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(shophub_core
    PUBLIC  Qt6::Core Qt6::Sql
    PRIVATE Qt6::Network)   # QPasswordDigestor (PBKDF2) lives in Qt Network
