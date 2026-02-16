include_guard(GLOBAL)

if (${REPO_OPTION_PREFIX}_ENABLE_CLIENT)
    opendaq_add_required_boost_libs(
        uuid
    )
endif()
