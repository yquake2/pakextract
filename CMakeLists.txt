set(PakextractFiles
    "pakextract.c"
    )

add_executable("pakextract" ${PakextractFiles})

set_target_properties("pakextract" PROPERTIES PROJECT_LABEL "Pakextract Tool")


