# vcpkg port skeleton for Quiver (REQ-BUILD-015). Consumes the standard CMake install/export
# (REQ-BUILD-009): configure -> install -> fixup the QuiverConfig package. Prepared at M8;
# registry submission is a release activity — REF and SHA512 below are set at submission time
# (`vcpkg x-add-version`), not committed here.
vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO div0rce/quiver
  REF "v${VERSION}"
  SHA512 0  # placeholder — computed at submission
  HEAD_REF main
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
  OPTIONS -DQUIVER_ENABLE_TESTS=OFF -DQUIVER_ENABLE_EXAMPLES=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME Quiver CONFIG_PATH lib/cmake/Quiver)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
