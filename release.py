import os
import sys
import shutil


# 复制文件
def copy_file(src, dst):
    try:
        if not os.path.exists(os.path.dirname(dst)):
            os.makedirs(os.path.dirname(dst))
        shutil.copy(src, dst)
        print(f"Copied {src} to {dst}")
    except IOError as e:
        print(f"Unable to copy file {src} to {dst}. {e}")


# 复制目录
def copy_directory(src, dst):
    try:
        if os.path.exists(dst):
            shutil.rmtree(dst)
        shutil.copytree(src, dst)
        print(f"Copied directory {src} to {dst}")
    except IOError as e:
        print(f"Unable to copy directory {src} to {dst}. {e}")


# 复制目录下指定后缀的文件
def copy_files_with_suffix(src_dir, dst_dir, suffix):
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith(suffix):
                src_file = os.path.join(root, file)
                dst_file = os.path.join(dst_dir, file)
                shutil.copy(src_file, dst_file)
                print(f"Copied {src_file} to {dst_file}")


# 删除文件
def delete_file(file_path):
    if os.path.exists(file_path):
        os.remove(file_path)
        print(f"Deleted file: {file_path}")
    else:
        print(f"File not found: {file_path}")

def release_win():
    # 1.复制doc路径下的下列文件夹：
    doc_dirs = ["qrest_data_lib", "svg"]
    for doc_dir in doc_dirs:
        copy_directory(os.path.join("doc", doc_dir), os.path.join("release", "win", "doc", doc_dir))
    # 2.复制doc路径下的下列文件：
    doc_files = ["qREST数据传输协议规范.md", "qREST数据传输协议规范.html", "qREST文件存储格式规范.md", "qREST文件存储格式规范.html"]
    for doc_file in doc_files:
        copy_file(os.path.join("doc", doc_file), os.path.join("release", "win", "doc", doc_file))
    # 3.复制example文件夹：
    copy_directory("example", os.path.join("release", "win", "example"))
    # 4.复制x64/Release路径下的导入库、动态库和可执行文件
    copy_files_with_suffix(os.path.join("x64", "Release"), os.path.join("release", "win", "lib"), ".lib")
    copy_files_with_suffix(os.path.join("x64", "Release"), os.path.join("release", "win", "bin"), ".dll")
    copy_files_with_suffix(os.path.join("x64", "Release"), os.path.join("release", "win", "bin"), ".exe")
    # 5.复制库的头文件
    copy_file(os.path.join("src", "qrest_data", "qrest_data.h"), os.path.join("release", "win", "include", "qrest_data", "qrest_data.h"))
    # 6.复制测试样例的源代码
    copy_file(os.path.join("src", "test_qrest_data", "test_qrest_data.cpp"), os.path.join("release", "win", "src", "test_qrest_data", "test_qrest_data.cpp"))
    # 7.复制readme和release说明文件
    copy_file("readme.md", os.path.join("release", "win", "readme.md"))
    copy_file("release.md", os.path.join("release", "win", "release.md"))

def release_linux():
    # 1.复制doc路径下的下列文件夹：
    doc_dirs = ["qrest_data_lib", "svg"]
    for doc_dir in doc_dirs:
        copy_directory(os.path.join("doc", doc_dir), os.path.join("release", "linux", "doc", doc_dir))
    # 2.复制doc路径下的下列文件：
    doc_files = ["qREST数据传输协议规范.md", "qREST数据传输协议规范.html", "qREST文件存储格式规范.md", "qREST文件存储格式规范.html"]
    for doc_file in doc_files:
        copy_file(os.path.join("doc", doc_file), os.path.join("release", "linux", "doc", doc_file))
    # 3.复制example文件夹：
    copy_directory("example", os.path.join("release", "linux", "example"))
    # 4.复制out路径下linux文件夹下的bin和lib文件夹
    copy_directory(os.path.join("out", "linux", "bin"), os.path.join("release", "linux", "bin"))
    copy_directory(os.path.join("out", "linux", "lib"), os.path.join("release", "linux", "lib"))
    # 5.复制库的头文件
    copy_file(os.path.join("src", "qrest_data", "qrest_data.h"), os.path.join("release", "linux", "include", "qrest_data", "qrest_data.h"))
    # 6.复制测试样例的源代码
    copy_file(os.path.join("src", "test_qrest_data", "test_qrest_data.cpp"), os.path.join("release", "linux", "src", "test_qrest_data", "test_qrest_data.cpp"))
    # 7.复制readme和release说明文件
    copy_file("readme.md", os.path.join("release", "linux", "readme.md"))
    copy_file("release.md", os.path.join("release", "linux", "release.md"))


if __name__ == "__main__":
    release_win()
    release_linux()
