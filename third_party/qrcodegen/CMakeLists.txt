# Nayuki's QR Code generator (MIT License, see LICENSE), used unchanged.
# Source: https://github.com/nayuki/QR-Code-generator (cpp/qrcodegen.hpp, cpp/qrcodegen.cpp)
# ShopHub uses it to draw the QR code on invoices (src/core/common/QrCode.h).
add_library(qrcodegen STATIC qrcodegen.hpp qrcodegen.cpp)
target_include_directories(qrcodegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(qrcodegen PROPERTIES POSITION_INDEPENDENT_CODE ON)
