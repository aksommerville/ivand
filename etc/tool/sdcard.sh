#!/bin/bash

IMAGE=out/sdcard.fat
ASMDIR=mid/sdcard
MDIR=mid/fatmount
PACKAGES="$1"
SUDO=sudo

if [ -n "$PACKAGES" ] ; then
  if ! [ -f "$PACKAGES" ] && ! [ -d "$PACKAGES" ] ; then
    echo "$PACKAGES: Not a regular file or directory"
    exit 1
  fi
fi

#---------------------------------------------
# Interactively prompt for other packages. (optional)

while true ; do

  echo ""
  echo "Packages to include:"
  for PACKAGE in $PACKAGES ; do
    echo "  $PACKAGE"
  done

  echo ""
  read -ep "Additional package or empty to proceed: " NEXTPACKAGE
  echo ""
  
  if [ -z "$NEXTPACKAGE" ] ; then
    break
  fi
  if [ -f "$NEXTPACKAGE" ] ; then
    if [ "${NEXTPACKAGE/*.}" != "zip" ] ; then
      echo "$NEXTPACKAGE: Must be a zip file"
      continue
    fi
  elif [ -d "$NEXTPACKAGE" ] ; then
    if ! [ -f "$NEXTPACKAGE/$(basename $NEXTPACKAGE).bin" ] ; then
      echo "$NEXTPACKAGE: Directories must contain a 'bin' file of the same name"
      continue
    fi
  else
    echo "$NEXTPACKAGE: Not a regular file or directory"
    continue
  fi
  PACKAGES="$PACKAGES $NEXTPACKAGE"
  history -s "$NEXTPACKAGE"
done

#-------------------------------------------------
# Generate the disk contents.

rm -rf "$ASMDIR"
mkdir -p "$ASMDIR" || exit 1

# Copy all the inputs.
cp -r $PACKAGES "$ASMDIR" || exit 1

# Any ZIP files, extract them, then delete the archive.
pushd "$ASMDIR" >/dev/null
for F in *.zip ; do
  unzip $F >/dev/null || exit 1
  rm $F
done
popd >/dev/null

# Determine the total size, then pad and round up for safety.
TOTALSIZE="$(du -sb "$ASMDIR" | cut -f1)"
TOTALSIZE="$(((TOTALSIZE+2000000)&~0xffff))"
if [ "$TOTALSIZE" -lt 16000000 ] ; then
  # There's a minimum size or something enforced by SDFat.h
  TOTALSIZE=16000000
fi

# Create the image file, at the full size but initially zeroed.
rm -f "$IMAGE"
dd if=/dev/zero of="$IMAGE" bs=$((1<<20)) count=$(((TOTALSIZE+1000000)/(1<<20))) 2>/dev/null || exit 1

# Format it with a FAT FS.
$SUDO mkfs.fat "$IMAGE" >/dev/null || exit 1

# Mount the image.
mkdir -p "$MDIR" || exit 1
$SUDO mount -oloop -tvfat "$IMAGE" "$MDIR" || exit 1

# Copy everything.
$SUDO cp -r "$ASMDIR"/* "$MDIR" || exit 1
$SUDO umount "$MDIR" || exit 1

# Clean up and report.
rm -r "$ASMDIR" "$MDIR"
echo "$IMAGE: Created FAT image for SD card."

#-------------------------------------------------------
# Optionally scan for valid block devices and offer to dd it.

IMAGESIZE="$(stat -c%s $IMAGE)"

echo "Devices we can write to:"
OK=

while read DEVPATH RM SIZE TYPE ; do
  if [ "$RM" -ne 1 ] ; then
    #echo "$DEVPATH: Not removable, ignore"
    continue
  fi
  if [ "$TYPE" != "part" ] ; then
    # Not sure this is a technical requirement.
    #echo "$DEVPATH: Not a partition, ignore"
    continue
  fi
  if [ "$SIZE" -lt "$IMAGESIZE" ] ; then
    #echo "$DEVPATH: Capacity $SIZE less than image $IMAGESIZE, ignore"
    continue
  fi
  echo "  $DEVPATH"
  OK=1
done < <( lsblk -nbro PATH,RM,SIZE,TYPE | sed 's/  / _ /g' )

if [ -z "$OK" ] ; then
  echo "...none. Find a valid SD card, and you can copy $IMAGE to it manually later."
  exit 1
fi

read -ep "Device path: " DEVPATH
if [ -z "$DEVPATH" ] ; then
  echo "Cancelled."
  exit 0
fi
if ! [ -b "$DEVPATH" ] ; then
  echo "That is not one of the options. Or it just lately stopped being a block device, or something."
  exit 1
fi

MOUNTPOINT="$(findmnt -S "$DEVPATH" -o TARGET -n)"
if [ -n "$MOUNTPOINT" ] ; then
  echo "$DEVPATH is currently mounted at $MOUNTPOINT:"
  ls "$MOUNTPOINT"
  read -ep "OK to unmount and overwrite? [y/N] " OK
  if [ "$OK" != "y" ] ; then
    echo "Cancelled."
    exit 0
  fi
  $SUDO umount "$DEVPATH" || exit 1
fi

echo ""
echo "We are about to do this as root:"
echo "  dd if=\"$IMAGE\" of=\"$DEVPATH\""
echo '!!!!! Last chance to abort !!!!!'
read -ep "Proceed? [y/N] " OK
if [ "$OK" != "y" ] ; then
  echo "Cancelled."
  exit 0
fi

$SUDO dd if="$IMAGE" of="$DEVPATH" 2>/dev/null || exit 1

echo "Copied image $IMAGE to device $DEVPATH."
