# Setup

Windows
```powershell
python -m venv venv
.\venv\Scripts\activate
```

macOS/Linux
```bash
python3 -m venv venv
source ./venv/Scripts/activate
```

## Install Packages
```
pip install -r .\scripts\requirements.txt 
```

# Running

```powershell
Get-ChildItem -Filter ./assets/test_images/*.png | ForEach-Object { python ./scripts/convert_to_rgb565.py $_ -f bin }
```