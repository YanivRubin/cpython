#!/usr/bin/python3
import os
import shutil

BOLD_GREEN = "\033[1;32m"
RESET = "\033[0m"

def notice(text: str):
    print(f"{BOLD_GREEN}{text}{RESET}")

def prompt_yes_no(prompt: str, default: bool) -> bool:
    full_prompt = prompt
    if default:
        full_prompt += " ([y]es/no): "
    else:
        full_prompt += " (yes/[n]o): "

    while True:
        user_input = input(full_prompt).strip().lower()
        if user_input in ["yes", "y"]:
            return True
        elif user_input in ["no", "n"]:
            return False
        elif user_input == "":
            return default
        else:
            print("Invalid input. Please enter 'yes' or 'no'.")

def run_bash(cmd:str):
    res = os.system(cmd)
    if res != 0:
        raise ValueError(f"error in bash: '{cmd}' returned {res}")

if __name__ == "__main__":
    if os.path.basename(os.getcwd()) != "cpython":
        raise ValueError("compilation must run from cpython base repo directory")

    if os.getuid() != 0:
        raise ValueError("must run make_script.py as sudo!")

    should_configure = prompt_yes_no("Configure?", False)
    should_clean = prompt_yes_no("Clean Build?", False)
    should_make = prompt_yes_no("Make?", True)
    should_install = prompt_yes_no("Install?", True)

    BUILD_DIR = os.path.join(os.getcwd(), "Build")
    if should_configure:
        notice("configuring!")
        run_bash(f"./configure --with-pydebug --prefix={BUILD_DIR}")

    if should_clean:
        notice("deleting Build dir")
        shutil.rmtree(BUILD_DIR)
        notice("make clean")
        run_bash("make clean")
        notice("recreating build dir")
        os.mkdir(BUILD_DIR)

    if should_make:
        notice("making!")
        run_bash("make -j")

    if should_install:
        notice("installing!")
        run_bash("sudo make install")

    notice(f"python compiled to {os.path.join(BUILD_DIR, "bin", "python3")}")
