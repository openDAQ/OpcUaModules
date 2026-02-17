include_guard(GLOBAL)

if (${REPO_OPTION_PREFIX}_ENABLE_CLIENT)
    opendaq_append_required_boost_components(
        uuid
    )
endif()
