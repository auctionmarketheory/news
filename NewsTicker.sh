#!/bin/bash
# Thiết lập đường dẫn tương đối tới thư mục chứa app
DIR="$(dirname "$0")/App_NewsTicker"

# Chuyển hướng thư mục làm việc để SDL có thể tải các resource tương đối (res/*)
cd "$DIR" || exit 1

# Khởi chạy ứng dụng và ghi log
./App_NewsTicker > log.txt 2>&1
