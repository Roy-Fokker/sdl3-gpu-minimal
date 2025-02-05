# Distributed under the OSI-approved BSD 3-Clause License.

#.rst:
# FindDds-ktx
# ------------
#
# Find the dds-ktx include header.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ``Dds_ktx_FOUND``
#   True if dds-ktx library found
#
# ``Dds_ktx_INCLUDE_DIR``
#   Location of dds-ktx header
#

include(${CMAKE_ROOT}/Modules/FindPackageHandleStandardArgs.cmake)
include(${CMAKE_ROOT}/Modules/SelectLibraryConfigurations.cmake)

if(NOT Dds-ktx_INCLUDE_DIR)
  find_path(Dds-ktx_INCLUDE_DIR NAMES dds-ktx.h PATHS ${Dds_ktx_DIR} PATH_SUFFIXES include)
endif()

find_package_handle_standard_args(Dds-ktx DEFAULT_MSG Dds-ktx_INCLUDE_DIR)
mark_as_advanced(Dds-ktx_INCLUDE_DIR)