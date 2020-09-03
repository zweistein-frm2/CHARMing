#!/bin/sh
GIT_DIFF=$(bash -c "git diff --quiet --exit-code || echo +")



echo echo "#!/bin/sh" >  version.cpp


echo "const char* GIT_REV=\"${GIT_REV}${GIT_DIFF}\"; >> version.cpp
#const char* GIT_TAG=\"${GIT_TAG}\";
#const char* GIT_BRANCH=\"${GIT_BRANCH}\";
#const char* GIT_DATE=\"${GIT_DATE}\";")
