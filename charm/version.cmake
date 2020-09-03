
if ("${repository}" STREQUAL "")
set(repository ${CMAKE_CURRENT_SOURCE_DIR})
endif()
message(****"version.cmake repository=" ${repository})

execute_process(COMMAND git log --pretty=format:'%h' -n 1
                OUTPUT_VARIABLE GIT_REV
                WORKING_DIRECTORY ${repository}
                ERROR_QUIET)

# Check whether we got any revision (which isn't
# always the case, e.g. when someone downloaded a zip
# file from Github instead of a checkout)
if ("${GIT_REV}" STREQUAL "")
    set(GIT_REV "")
    set(GIT_DIFF "")
    set(GIT_TAG "")
	set(GIT_DESCRIBE_TAGS "")
    set(GIT_LATEST_TAG "")
    set(GIT_NUMBER_OF_COMMITS_SINCE "")
    set(GIT_BRANCH "")
    set(GIT_DATE "")
    set(GIT_PATCH "")
else()
    execute_process(
        COMMAND bash -c "git diff --quiet --exit-code || echo +"
         WORKING_DIRECTORY ${repository}
        OUTPUT_VARIABLE GIT_DIFF)
    execute_process(
        COMMAND git describe --exact-match --tags
         WORKING_DIRECTORY ${repository}
        OUTPUT_VARIABLE GIT_TAG ERROR_QUIET)
	execute_process(
        COMMAND git describe --tags
         WORKING_DIRECTORY ${repository}
        OUTPUT_VARIABLE GIT_DESCRIBE_TAGS ERROR_QUIET)
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
         WORKING_DIRECTORY ${repository}
        OUTPUT_VARIABLE GIT_BRANCH)
    execute_process(
        COMMAND git describe --abbrev=0 --tags
        WORKING_DIRECTORY ${repository}
        OUTPUT_VARIABLE GIT_LATEST_TAG ERROR_QUIET)

    string(STRIP "${GIT_LATEST_TAG}" GIT_LATEST_TAG)

    execute_process(
        COMMAND git rev-list --tags ${GIT_LATEST_TAG}..HEAD --count
        WORKING_DIRECTORY ${repository}
        OUTPUT_VARIABLE GIT_NUMBER_OF_COMMITS_SINCE)


    execute_process(
        COMMAND git diff HEAD
         WORKING_DIRECTORY ${repository}
        OUTPUT_VARIABLE GIT_DIFF_HEAD)


if ("${GIT_DIFF_HEAD}" STREQUAL "")
    execute_process(
        COMMAND git show -s --format=%cd --date=format:%y-%m-%dT%H_%M%z
         WORKING_DIRECTORY ${repository}
        OUTPUT_VARIABLE GIT_DATE)
    execute_process(
        COMMAND git show -s --format=%cd --date=format:%y%m%d
         WORKING_DIRECTORY ${repository}
        OUTPUT_VARIABLE GIT_PATCH)
else()
    string(TIMESTAMP CURRENT_TIME "%y-%m-%dT%H_%M")
    set(GIT_DATE Uncommmitted-${CURRENT_TIME})

     string(TIMESTAMP PATCH_TIME "%y%m%d%H%M")
    set(GIT_PATCH ${PATCH_TIME})

endif()

    if ("${GIT_LATEST_TAG}" STREQUAL "")
     set(GIT_LATEST_TAG 0)
      execute_process(
        COMMAND git rev-list  --count master
        WORKING_DIRECTORY ${repository}
        OUTPUT_VARIABLE GIT_NUMBER_OF_COMMITS_SINCE)

    endif()

    string(STRIP "${GIT_REV}" GIT_REV)
    string(SUBSTRING "${GIT_REV}" 1 7 GIT_REV)
    string(STRIP "${GIT_DIFF}" GIT_DIFF)
    string(STRIP "${GIT_TAG}" GIT_TAG)
    string(STRIP "${GIT_DESCRIBE_TAGS}" GIT_DESCRIBE_TAGS)
    string(STRIP "${GIT_NUMBER_OF_COMMITS_SINCE}" GIT_NUMBER_OF_COMMITS_SINCE)
    string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
    string(STRIP "${GIT_DATE}" GIT_DATE)
    string(STRIP "${GIT_PATCH}" GIT_PATCH)
endif()

set(VERSION "const char* GIT_REV=\"${GIT_REV}${GIT_DIFF}\";
const char* GIT_TAG=\"${GIT_TAG}\";
const char* GIT_LATEST_TAG=\"${GIT_LATEST_TAG}\";
const char* GIT_DESCRIBE_TAGS=\"${GIT_DESCRIBE_TAGS}\";
const char* GIT_NUMBER_OF_COMMITS_SINCE=\"${GIT_NUMBER_OF_COMMITS_SINCE}\";
const char* GIT_BRANCH=\"${GIT_BRANCH}\";
const char* GIT_DATE=\"${GIT_DATE}\";
const char* GIT_PATCH=\"${GIT_PATCH}\";")

set(VERSION_HEADER  "extern const char* GIT_REV;
extern const char* GIT_TAG;
extern const char* GIT_LATEST_TAG;
extern const char* GIT_DESCRIBE_TAGS;
extern const char* GIT_NUMBER_OF_COMMITS_SINCE;
extern const char* GIT_BRANCH;
extern const char* GIT_DATE;
extern const char* GIT_PATCH;"
)

if(EXISTS ${repository}/version.cpp)
    file(READ ${repository}/version.cpp VERSION_)
else()
    set(VERSION_ "")
endif()

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE ${repository}/version.cpp "${VERSION}")
    file(WRITE ${repository}/version.h "${VERSION_HEADER}")
    message(version.cmake: written:  ${repository}/version.cpp)
    message(${VERSION})
endif()


