# syntax=docker/dockerfile:1.7
#
# Commit-image Dockerfile (SMALL form, PREFERRED) — builds ON TOP of the org C/C++ base
# image (ghcr.io/savantenvs/base). The base already provides debian + clang/llvm +
# libFuzzer/afl++ + the identities + the build-contract ENV, so this only adds the
# per-commit build (built on top of the prebuilt base, rather than building the toolchain in-image).
#
# Build (from the repo root):
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile \
#       --build-arg COMMIT=$(git rev-parse HEAD) \
#       [--build-arg SOURCE_DATE_EPOCH=$(date +%s)] \
#       -t mytag .

# The org base, pinned to a fixed DATE-tagged base (YYYYMMDD; shared by all integrations
# minted against it). Use the current dated tag; pin by digest in production: ...base@sha256:<digest>.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
# SOURCE_DATE_EPOCH stays an ARG (available to build.sh); do NOT bake it as an ENV — an
# empty value breaks clang ("must be a non-negative decimal integer") both here and for
# any in-image rebuild. build.sh unsets it when empty.
ARG SOURCE_DATE_EPOCH

# Sanitizer override: the base exports SANITIZER_FLAGS (ASan+UBSan, halting) as the default.
# To change it for a build (e.g. ASan-only, or add MSan): --build-arg SANITIZER_FLAGS="...".
# Declare it WITHOUT a default (no '='): an undefaulted ARG lets the base ENV flow through to
# build.sh when not passed, and an explicit --build-arg overrides it. (An empty default — `=` —
# would instead shadow the base ENV and silently wipe the sanitizers.)
ARG SANITIZER_FLAGS
# Coverage (optional, off by default): instrument the TEST build to measure how much of the project
# the suite exercises — --build-arg COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping".
# Empty by default (no effect on the normal oracle build); build.sh appends it to the test build only.
ARG COVERAGE_FLAGS=
# Build parallelism (optional). build.sh falls back to nproc when unset: --build-arg MAYHEM_JOBS=8
ARG MAYHEM_JOBS=

# If mayhem/build.sh needs EXTRA apt packages, install them as root before the build
# (re-pin to SOURCE_DATE_EPOCH first for reproducibility — repro-sources-list.sh ships
# in the base):
#   USER root
#   RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
#       apt-get update && apt-get install -yq --no-install-recommends <pkgs> \
#    && rm -rf /var/lib/apt/lists/*
#   USER mayhem

# opencv_contrib's modules only build inside OpenCV's own build system, against OpenCV proper. OpenCV
# (the pinned 5.x commit contemporary with this contrib tree) is built ONCE here, uninstrumented and
# static, into /opt/toolchains/deps/opencv-5.x-71babee3/{include,lib}; its source stays in src/ so
# backtraces show those paths. mayhem/build.sh then compiles the contrib modules (the target) with the
# fuzz flags against it, out of tree (SPEC §6.2 item 19). cvconfig.h and cv_cpu_config.h are generated in
# OpenCV's build tree and not installed, but the contrib sources need them, so they are copied next to
# the installed headers.
USER root
ARG OPENCV_COMMIT=71babee383e846ce4a037a7aa4a92c72551c4866
ARG OPENCV_SHA256=a75d913d17bebee46e6408229f336e2d76a3046e1dd56ea24fc0d362995d3785
RUN d=/opt/toolchains/deps/opencv-5.x-71babee3 && mkdir -p "$d/src" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz https://github.com/opencv/opencv/archive/${OPENCV_COMMIT}.tar.gz \
 && echo "${OPENCV_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" && cd /tmp/dep-build \
 && cmake -G Ninja "$d/src" -DCMAKE_INSTALL_PREFIX="$d" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_ASM_COMPILER=clang \
      -DCMAKE_C_FLAGS="-O2 -g -gdwarf-3" -DCMAKE_CXX_FLAGS="-O2 -g -gdwarf-3" \
      -DBUILD_SHARED_LIBS=OFF -DBUILD_LIST=core,imgproc,imgcodecs,geometry,features,objdetect,dnn,video,stereo,flann,ml,photo,calib \
      -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF -DBUILD_opencv_apps=OFF \
      -DBUILD_JAVA=OFF -DBUILD_opencv_python3=OFF -DBUILD_opencv_python_bindings_generator=OFF \
      -DBUILD_PROTOBUF=ON -DBUILD_ZLIB=ON -DBUILD_PNG=ON -DBUILD_JPEG=ON -DENABLE_PRECOMPILED_HEADERS=OFF \
      -DWITH_IPP=OFF -DWITH_ITT=OFF -DWITH_TBB=OFF -DWITH_OPENCL=OFF -DWITH_EIGEN=OFF -DWITH_LAPACK=OFF \
      -DWITH_FFMPEG=OFF -DWITH_GSTREAMER=OFF -DWITH_GTK=OFF -DWITH_OPENEXR=OFF -DWITH_TIFF=OFF -DWITH_WEBP=OFF \
      -DWITH_JASPER=OFF -DWITH_OPENJPEG=OFF -DWITH_AVIF=OFF -DWITH_JPEGXL=OFF -DWITH_V4L=OFF -DWITH_1394=OFF \
      -DOPENCV_GENERATE_PKGCONFIG=OFF \
 && ninja -j"$(nproc)" install \
 && cp cvconfig.h cv_cpu_config.h "$d/include/opencv5/" \
 && cd / && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d"
USER mayhem

# Source enters from the build CONTEXT (the CI checkout): the FULL repo (incl. .git
# history) is copied to /mayhem as `mayhem`; build.sh builds in place.
USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

# Anything a Dockerfile step materializes into the tree (downloaded fixtures, generated headers,
# vendored deps) MUST be committed or (re)created by build.sh: rlenv's patch grader runs
# `git clean -ffdX` before every graded build, so a gitignored, untracked file is gone by then and
# every honest patch scores zero (SPEC §6.2 item 16; verify-repo.sh checks it).
#
# OPTIONAL compile-cache pre-fill (SPEC §6.2 item 18). Bases minted with /opt/mayhem/ccache/prefill
# run the build under rlenv's pinned ccache, so the patch grader's first (otherwise cold) rebuild is
# warm — this is what keeps large C/C++ targets inside the build time limit. Older bases fall back to
# the plain build; a missing or stale pre-fill only costs a cold cache. Never enable ccache in
# build.sh or via ENV/PATH, and keep the compiler resolving by name (CC=clang) or nothing is cached.
RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

# Run the project's functional tests at build time IF the repo ships them (mayhem/test.sh is
# optional — omit it only when the repo has no tests meaningful for RL). A failing suite fails the
# image build, so the commit image is guaranteed to build AND pass its own tests.
RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem" \
      rlenv.fix="09947aa470b9e4c798bac8a4a64f0eac259e8736"

USER mayhem
WORKDIR /mayhem
