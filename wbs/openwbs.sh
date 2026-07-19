chcp 65001
DIR=`pwd`
DIR=`echo $DIR | sed 's/^.//; s/\//:\//'`
echo $DIR
"C:\Program Files\Google\Chrome\Application\chrome.exe" --allow-file-access-from-files file://$DIR/wbs_viewer_mod.html
