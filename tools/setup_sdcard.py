import os
import shutil
import sys

DEST = "K:/"
REPO_SD = r"C:\Users\cyphe\meowkit-workspace\sd files"
POOM_SD = r"C:\Users\cyphe\poom-workspace\sdcard"

def main():
    print(f"[*] Setting up MEOWKit MicroSD Card on {DEST}...")

    # 1. Base directories from repo
    for item in os.listdir(REPO_SD):
        src_path = os.path.join(REPO_SD, item)
        dst_path = os.path.join(DEST, item)
        if os.path.isdir(src_path):
            shutil.copytree(src_path, dst_path, dirs_exist_ok=True)
            print(f"[+] Synced tree: {item}")
        else:
            shutil.copy2(src_path, dst_path)
            print(f"[+] Copied file: {item}")

    # 2. Enhanced universal IR libraries from POOM lab
    poom_ir = os.path.join(POOM_SD, "ir")
    dest_ir = os.path.join(DEST, "infrared")
    if os.path.exists(poom_ir):
        for f in os.listdir(poom_ir):
            if f.endswith(".ir"):
                shutil.copy2(os.path.join(poom_ir, f), os.path.join(dest_ir, f))
                print(f"[+] Added Tactical IR remote: {f}")

    # 3. NFC card vault dumps
    poom_nfc = os.path.join(POOM_SD, "nfc")
    dest_nfc = os.path.join(DEST, "nfc")
    os.makedirs(dest_nfc, exist_ok=True)
    if os.path.exists(poom_nfc):
        for f in os.listdir(poom_nfc):
            shutil.copy2(os.path.join(poom_nfc, f), os.path.join(dest_nfc, f))
            print(f"[+] Staged NFC card: {f}")

    # 4. Custom DarkCyfr BadUSB payloads
    scripts_dir = os.path.join(DEST, "badusb", "scripts")
    os.makedirs(scripts_dir, exist_ok=True)

    recon_script = (
        "REM DarkCyfr Host Recon\n"
        "DEFAULT_DELAY 100\n"
        "GUI r\n"
        "DELAY 400\n"
        "STRING powershell -NoP -C \"$Host.UI.RawUI.WindowTitle='DarkCyfr Recon'; Get-ComputerInfo | Select-Object WindowsProductName, CsManufacturer, CsModel, CsTotalPhysicalMemory | Format-List; ipconfig /all | Select-String 'IPv4|Default Gateway|Description'; Read-Host 'Recon complete. Press ENTER to close'\"\n"
        "ENTER\n"
    )
    with open(os.path.join(scripts_dir, "darkcyfr_recon.txt"), "w", encoding="utf-8") as f:
        f.write(recon_script)
    print("[+] Created BadUSB payload: darkcyfr_recon.txt")

    matrix_script = (
        "REM Cyberpunk Matrix Terminal\n"
        "DEFAULT_DELAY 100\n"
        "GUI r\n"
        "DELAY 400\n"
        "STRING powershell -NoP -C \"$Host.UI.RawUI.ForegroundColor='Green'; $Host.UI.RawUI.BackgroundColor='Black'; Clear-Host; while($true){ -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 80 | ForEach-Object {[char]$_}) }\"\n"
        "ENTER\n"
    )
    with open(os.path.join(scripts_dir, "matrix_screen.txt"), "w", encoding="utf-8") as f:
        f.write(matrix_script)
    print("[+] Created BadUSB payload: matrix_screen.txt")

    # 5. Standard directories for future modules & capture logs
    for extra_dir in ["apps", "logs", "pcaps", "wallpapers"]:
        path = os.path.join(DEST, extra_dir)
        os.makedirs(path, exist_ok=True)
        print(f"[+] Initialized directory: /{extra_dir}/")

    print("\n[OK] MicroSD Card Provisioning Complete!")

if __name__ == "__main__":
    main()
