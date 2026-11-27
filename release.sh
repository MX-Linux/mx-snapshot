#!/usr/bin/env bash
# Copyright (C) 2017-2026 MX Authors. License: GPL-3.0-or-later
set -euo pipefail
unset CDPATH
shopt -s nullglob

die() { echo "release: $*" >&2; exit 1; }
confirm() { local reply; read -r -p "$1 [y/N] " reply && [[ $reply =~ ^[Yy]([Ee][Ss])?$ ]]; }
usage() {
    cat <<'HELP'
Usage: release [OPTIONS] [VERSION]
Create or reuse an annotated release tag and publish the corresponding AUR recipe.
Omit VERSION to reuse the latest version tag. Run inside the project checkout.
  -n, --no-push       Commit the AUR update locally. The project tag is still pushed.
  -r, --remote NAME   Git remote for the project tag (detected from the recipe)
  -a, --aur-dir DIR   Separate AUR checkout (default: aur)
      --tag-only      Publish just the project tag, including for projects without AUR
  -h, --help          Show this help
AUR_PUSH=0 also skips the AUR push. AUR_REMOTE, AUR_LOCAL_BRANCH and
AUR_REMOTE_BRANCH default to aur, main and master respectively.
HELP
}

tag=
remote=${TAG_REMOTE:-}
aur=${AUR_DIR:-aur}
push=${AUR_PUSH:-1}
aur_remote=${AUR_REMOTE:-aur}
aur_branch=${AUR_LOCAL_BRANCH:-main}
publish_branch=${AUR_REMOTE_BRANCH:-master}
tag_only=false
while (($#)); do
    case $1 in
        -h|--help) usage; exit ;;
        -n|--no-push) push=0 ;;
        --tag-only) tag_only=true ;;
        -r|--remote|-a|--aur-dir)
            [[ -n ${2:-} && $2 != -* ]] || die "$1 requires a value"
            if [[ $1 == -r || $1 == --remote ]]; then remote=$2; else aur=$2; fi
            shift ;;
        --remote=*) remote=${1#*=} ;;
        --aur-dir=*) aur=${1#*=} ;;
        --force|--update) echo "$1 is redundant. Existing tags are reused." >&2 ;;
        -*) die "unknown argument: $1" ;;
        *) [[ -z $tag ]] || die "only one version may be given"; tag=$1 ;;
    esac
    shift
done
version_re='^v?[0-9]+\.[0-9]+(\.[0-9]+)?[a-zA-Z0-9]*$'
[[ -z $tag || $tag =~ $version_re ]] || die "Invalid version format: $tag. Expected formats include 26.01, v1.0.0 and 26.01arch."
root=$(git rev-parse --show-toplevel) || die "run inside a project checkout"
cd -- "$root"
exec 8<"$root"
flock -x 8
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

# Extract owner/repository from HTTPS, SSH and git+https GitHub URLs.
github_repo() {
    local re='github\.com[:/]([a-zA-Z0-9_.-]+/[a-zA-Z0-9_.-]+)'
    [[ $1 =~ $re ]] || return 1
    local slug=${BASH_REMATCH[1]}; printf '%s\n' "${slug%.git}"
}
repo=
if [[ $tag_only == false && -f $aur/PKGBUILD ]]; then
    info=$(cd "$aur" && makepkg --printsrcinfo)
    while read -r key _ value; do
        [[ $key == source ]] || continue
        if repo=$(github_repo "$value"); then break; fi
    done <<< "$info"
    if [[ -z $repo ]]; then repo=$(github_repo "$(awk '$1=="url" {print $3; exit}' <<< "$info")") || die "no GitHub source or URL in $aur/PKGBUILD"; fi
fi
mapfile -t remotes < <(git remote)
if [[ -z $remote ]]; then
    for candidate in "${remotes[@]}"; do
        slug=$(github_repo "$(git remote get-url --push "$candidate")") || continue
        if [[ -n $repo && ${slug,,} == "${repo,,}" ]]; then remote=$candidate; break; fi
    done
    if [[ -z $remote && ${#remotes[@]} -eq 1 ]]; then
        candidate=${remotes[0]}
        slug=$(github_repo "$(git remote get-url --push "$candidate")") || slug=
        [[ -z $repo || -z $slug || ${slug,,} == "${repo,,}" ]] || die "no Git remote matches $repo"
        remote=$candidate
    fi
    [[ -n $remote ]] || die "choose the project remote with --remote"
fi
git remote get-url "$remote" >/dev/null || die "unknown remote: $remote"
# Query the push destination in case the remote has a distinct fetch URL.
destination=$(git remote get-url --push "$remote")
git ls-remote --tags --refs "$destination" > "$work/tags" || die "Could not check tag on $remote"
git tag -l > "$work/names"
sed 's|^.*refs/tags/||' "$work/tags" >> "$work/names"
latest=$(awk '/^v?[0-9]+\.[0-9]+(\.[0-9]+)?[a-zA-Z0-9]*$/ {v=$0; sub(/^v/,"",v); print v, $0}' "$work/names" | LC_ALL=C sort -k1,1V -k2,2 | tail -1 | awk '{print $2}')
if [[ -z $tag ]]; then
    [[ -n $latest ]] || die "no version tags found. Specify a version"
    tag=$latest
    echo "No version specified; using latest tag '$tag'"
fi
branch=$(git branch --show-current)
if [[ $branch != "${MAIN_BRANCH:-main}" && ( -n ${MAIN_BRANCH:-} || $branch != master ) ]]; then
    confirm "Currently on '$branch'. Continue?" || exit 0
fi

ref=refs/tags/$tag
pkgver=${tag#v}
remote_oid=$(awk -v ref="$ref" '$2==ref {print $1}' "$work/tags")
local_oid=$(git rev-parse -q --verify "$ref" || true)
if [[ -n $remote_oid && -n $local_oid && $remote_oid != "$local_oid" ]]; then die "local and remote tag '$tag' differ"; fi
new=false
if [[ -z $remote_oid && -z $local_oid ]]; then
    new=true
    if [[ -n $latest ]]; then
        last=${latest#v}
        [[ $pkgver != "$last" && $(printf '%s\n' "$pkgver" "$last" | sort -V | tail -1) == "$pkgver" ]] || die "$tag must be newer than $latest"
    fi
fi

if [[ $tag_only == false ]]; then
    [[ -f $aur/PKGBUILD && -n $repo ]] || die "missing AUR checkout at $aur. Use --tag-only for a tag release"
    aur=$(realpath -- "$aur")
    [[ $(git -C "$aur" rev-parse --show-toplevel) == "$aur" && $aur != "$root" ]] || die "$aur must be a separate checkout"
    [[ $(git -C "$aur" branch --show-current) == "$aur_branch" ]] || die "expected $aur_branch in $aur"
    if [[ $push != 0 ]]; then
        git -C "$aur" fetch --quiet "$aur_remote" "$publish_branch"
        git -C "$aur" merge-base --is-ancestor FETCH_HEAD HEAD || die "AUR checkout is behind or diverged"
    fi
    stage=$work/recipe
    mkdir "$stage"
    old_version=$(awk '$1=="pkgver" {print $3; exit}' <<< "$info")
    [[ $(printf '%s\n' "$old_version" "$pkgver" | sort -V | tail -1) == "$pkgver" ]] || die "AUR is already at newer version $old_version"
    # Prepare the entire update outside the AUR checkout. Only recipe paths go into the commit.
    git -C "$aur" archive HEAD | tar -x -C "$stage"
    files=(PKGBUILD .SRCINFO)
    if [[ -f arch/PKGBUILD ]]; then
        if ! grep -Fxq "pkgver=$pkgver" arch/PKGBUILD || ! git diff --quiet HEAD -- arch ||
            ! git for-each-ref --contains=HEAD --format='%(refname)' "refs/remotes/$remote" | grep . >/dev/null; then
            if [[ $new == true ]]; then die "set arch/PKGBUILD to $pkgver, commit and push arch/ to $remote before tagging"; fi
            echo "Warning: check the OBS recipe and branch for $tag" >&2
        fi
        if [[ $new == true ]] && git ls-files -- 'debs/*.tar.xz' | grep . >/dev/null; then
            obs_info=$(cd arch && makepkg --printsrcinfo)
            obs_archive=$(awk '$1=="source" && $3 ~ /\.tar\.xz$/ {print $3; exit}' <<< "$obs_info")
            if [[ -n $obs_archive ]]; then
                git cat-file -e "HEAD:debs/$obs_archive" || die "missing committed OBS source archive debs/$obs_archive"
            fi
        fi
        # For a retry, use the recipe that belongs to the release, even after the branch advances.
        archive_ref=HEAD
        if [[ -n $remote_oid && -z $local_oid ]]; then git fetch --no-tags "$destination" "$ref:$ref"; local_oid=$remote_oid; fi
        [[ -z $local_oid ]] || archive_ref=$ref
        if git cat-file -e "$archive_ref:arch/PKGBUILD" 2>/dev/null; then
            git archive "$archive_ref" arch | tar -x -C "$work"
            for file in "$work"/arch/*; do
                [[ -f $file ]] || continue
                name=${file##*/}
                cp -- "$file" "$stage/$name"
                [[ $name == PKGBUILD ]] || files+=("$name")
            done
        fi
    fi
    git -C "$aur" diff --quiet HEAD -- "${files[@]}" || die "AUR recipe files have uncommitted changes"
    for name in "${files[@]}"; do
        if [[ -e $aur/$name ]] && ! git -C "$aur" ls-files --error-unmatch -- "$name" >/dev/null 2>&1; then die "untracked AUR file would be overwritten: $name"; fi
    done
fi

if [[ $new == true ]]; then
    if ! git for-each-ref --contains=HEAD --format='%(refname)' "refs/remotes/$remote" | grep . >/dev/null; then
        confirm "HEAD is on no known $remote branch. Continue?" || exit 0
    fi
    if [[ -n ${EDITOR:-} ]]; then
        printf '## Release %s\n\n- Describe the changes\n' "$tag" > "$work/template"
        cp "$work/template" "$work/notes"
        read -ra editor <<< "$EDITOR"
        "${editor[@]}" "$work/notes"
        cmp -s "$work/template" "$work/notes" && die "edit the release annotation before tagging"
    else
        read -r -p 'Release annotation: ' annotation
        printf '%s\n' "$annotation" > "$work/notes"
    fi
    grep -q '[^[:space:]]' "$work/notes" || die "empty release annotation"
    cat "$work/notes"
    confirm "Create tag $tag and push it to $remote?" || exit 0
    git tag -a "$tag" -F "$work/notes"
elif [[ -z $local_oid ]]; then
    git fetch --no-tags "$destination" "$ref:$ref"
fi
if [[ -z $remote_oid ]]; then git push "$remote" "$ref:$ref"; fi
[[ $tag_only == false ]] || exit 0
annotation=$(git for-each-ref --format='%(contents)' "$ref")
annotation=${annotation:-Update to $tag}
url=https://github.com/$repo/archive/refs/tags/$tag.tar.gz
curl -fLsS --retry 4 --retry-all-errors --retry-delay 5 -o "$work/archive" "$url"
[[ -s $work/archive ]] || die "empty tag archive"
if git cat-file -e "$ref:CMakeLists.txt" 2>/dev/null; then
    tar -tzf "$work/archive" -- "${repo##*/}-$pkgver/CMakeLists.txt" >/dev/null || die "tag archive has an unexpected source directory"
fi
checksum=$(sha256sum "$work/archive"); checksum=${checksum%% *}

# Rewrite only the project's archive in source=, retaining local hooks and other sources.
base_info=$(cd "$stage" && makepkg --printsrcinfo)
URL=$url REPO=$repo VERSION=$pkgver OLD_VERSION=$old_version perl -0777 -i -pe '
    s/^pkgver=.*$/pkgver=$ENV{VERSION}/m or s/^(pkgname=.*\n)/${1}pkgver=$ENV{VERSION}\n/m;
    s/^pkgrel=.*$/pkgrel=1/m if $ENV{OLD_VERSION} ne $ENV{VERSION};
    s/^pkgver\(\)\s*\{.*?^\}\n//ms;
    s{^(source=\()(.*?)(\))}{
        my ($open,$sources,$close)=($1,$2,$3);
        $sources =~ s{(["\x27]?)(?:[^\s"\x27()]*::)?(?:git\+)?https://github\.com/\Q$ENV{REPO}\E(?:\.git)?(?:/archive/[^\s"\x27()]*)?(?:#[^\s"\x27()]*)?\1}{"$ENV{URL}"}i
            or $sources =~ s{(["\x27]?)[^\s"\x27()/:]+(?:_\$\{?pkgver\}?\.tar\.xz|-source\.tar)\1}{"$ENV{URL}"};
        "$open$sources$close"
    }mse;
    my ($name)=$ENV{REPO}=~m{/([^/]+)$};
    s/^_srcdir=.*$/_srcdir="$name-$ENV{VERSION}"/m;
    if (!/^source=\([^)]*git\+/m) {
        s{^(makedepends=\()(.*?)(\))}{
            my ($open,$deps,$close)=($1,$2,$3);
            $deps =~ s{(?<!\S)(["\x27]?)git\1(?=\s|$)}{}g;
            "$open$deps$close"
        }mse;
    }
' "$stage/PKGBUILD"
metadata=$(cd "$stage" && makepkg --printsrcinfo)
mapfile -t sources < <(awk '$1=="source" {print $3}' <<< "$metadata")
mapfile -t sums < <(awk '$1=="sha256sums" {print $3}' <<< "$base_info")
[[ ${#sources[@]} == "${#sums[@]}" ]] || die "expected one SHA256 checksum per source"
found=false
for i in "${!sources[@]}"; do
    location=${sources[i]#*::}
    if [[ $location == "$url" ]]; then
        sums[i]=$checksum; found=true
    elif [[ $location != *://* ]]; then
        hash=$(sha256sum "$stage/$location"); sums[i]=${hash%% *}
    fi
done
[[ $found == true ]] || die "could not identify the project archive in PKGBUILD"
SUMS="${sums[*]}" perl -0777 -i -pe 's{^sha256sums=\(.*?\)}{"sha256sums=(" . join(" ",map { "\x27$_\x27" } split / /,$ENV{SUMS}) . ")"}mse' "$stage/PKGBUILD"
(cd "$stage" && makepkg --printsrcinfo) > "$stage/.SRCINFO"
awk -v v="$pkgver" -v c="$checksum" '$1=="pkgver" && $3==v {version=1} $1=="sha256sums" && $3==c {sum=1} END {exit !(version && sum)}' "$stage/.SRCINFO" || die "inconsistent generated .SRCINFO"

# Roll back only the release paths if installing or committing the prepared files fails.
install_recipe() (
    local previous name
    local existing=() added=()
    previous=$(git -C "$aur" rev-parse HEAD)
    git -C "$aur" diff --quiet HEAD -- "${files[@]}" || die "AUR recipe changed during the release"
    for name in "${files[@]}"; do
        if git -C "$aur" ls-files --error-unmatch -- "$name" >/dev/null 2>&1; then
            existing+=("$name")
        else
            [[ ! -e $aur/$name && ! -L $aur/$name ]] || die "AUR file appeared during the release: $name"
            added+=("$name")
        fi
    done
    # Invoked by the EXIT trap.
    # shellcheck disable=SC2317
    restore_recipe() {
        if [[ $(git -C "$aur" rev-parse HEAD) == "$previous" ]]; then
            if (( ${#existing[@]} )); then git -C "$aur" restore --source="$previous" --staged --worktree -- "${existing[@]}"; fi
            if (( ${#added[@]} )); then git -C "$aur" rm -fq --ignore-unmatch -- "${added[@]}"; fi
            for name in "${added[@]}"; do rm -f -- "$aur/$name"; done
        fi
    }
    trap restore_recipe EXIT
    for name in "${files[@]}"; do cp -- "$stage/$name" "$aur/$name"; done
    git -C "$aur" add -- "${files[@]}"
    if ! git -C "$aur" diff --cached --quiet -- "${files[@]}"; then
        git -C "$aur" commit --only -m "$annotation" -- "${files[@]}"
    fi
    trap - EXIT
)
install_recipe
if [[ $push == 0 ]]; then
    printf 'To publish: git -C %q push %q %q\n' "$aur" "$aur_remote" "$aur_branch:$publish_branch"
else
    git -C "$aur" push "$aur_remote" "$aur_branch:$publish_branch"
fi
