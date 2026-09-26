import os
import subprocess
import markdown

script_dir = os.path.dirname(os.path.abspath(__file__))
project_dir = os.path.dirname(script_dir)
md_path = os.path.join(project_dir, "Manuals", "Technical_Game_Design.md")
html_path = os.path.join(project_dir, "Manuals", "Technical_Game_Design.html")
pdf_path_manuals = os.path.join(project_dir, "Manuals", "Technical_Game_Design.pdf")
pdf_path_root = os.path.join(project_dir, "TechnicalGameDesign.pdf")

with open(md_path, "r", encoding="utf-8") as f:
    md_content = f.read()

# Convert Markdown to HTML
html_body = markdown.markdown(
    md_content,
    extensions=["tables", "fenced_code", "toc"]
)

# Custom Print CSS for elegant A4 documentation
full_html = f"""<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<title>Documentation Technique d'Implémentation — Rébus &amp; Indices</title>
<style>
    @page {{
        size: A4;
        margin: 20mm 15mm 20mm 15mm;
        @bottom-right {{
            content: counter(page);
        }}
    }}
    
    body {{
        font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
        font-size: 10pt;
        line-height: 1.5;
        color: #222;
        margin: 0;
        padding: 0;
    }}
    
    h1 {{
        font-size: 17pt;
        font-weight: 700;
        margin-top: 0;
        margin-bottom: 6px;
        color: #111;
        border-bottom: 2px solid #333;
        padding-bottom: 6px;
    }}
    
    h2 {{
        font-size: 13pt;
        font-weight: 700;
        margin-top: 18pt;
        margin-bottom: 6pt;
        color: #1a2a3a;
        border-bottom: 1px solid #ccc;
        padding-bottom: 3px;
        page-break-after: avoid;
    }}
    
    h3 {{
        font-size: 11pt;
        font-weight: 600;
        margin-top: 12pt;
        margin-bottom: 4pt;
        color: #2c3e50;
        page-break-after: avoid;
    }}
    
    blockquote {{
        border-left: 4px solid #0088cc;
        background-color: #f7f9fa;
        margin: 10px 0;
        padding: 8px 12px;
        font-size: 9.5pt;
        color: #444;
    }}
    
    blockquote p {{
        margin: 4px 0;
    }}
    
    table {{
        width: 100%;
        border-collapse: collapse;
        margin: 12px 0;
        font-size: 8.5pt;
        page-break-inside: avoid;
    }}
    
    th, td {{
        border: 1px solid #d0d7de;
        padding: 5px 8px;
        text-align: left;
    }}
    
    th {{
        background-color: #f2f4f6;
        font-weight: 600;
        color: #24292f;
    }}
    
    tr:nth-child(even) {{
        background-color: #fafbfc;
    }}
    
    pre {{
        background-color: #f6f8fa;
        border: 1px solid #e1e4e8;
        border-radius: 4px;
        padding: 8px 10px;
        overflow-x: auto;
        font-family: "Consolas", "Courier New", monospace;
        font-size: 8pt;
        line-height: 1.35;
        margin: 8px 0;
        page-break-inside: avoid;
    }}
    
    code {{
        font-family: "Consolas", "Courier New", monospace;
        font-size: 8.5pt;
        background-color: #f0f2f4;
        padding: 1px 4px;
        border-radius: 3px;
    }}
    
    pre code {{
        background-color: transparent;
        padding: 0;
        border-radius: 0;
    }}
    
    hr {{
        border: none;
        border-top: 1px solid #e1e4e8;
        margin: 14pt 0;
    }}
    
    ul, ol {{
        margin-top: 4px;
        margin-bottom: 8px;
        padding-left: 20px;
    }}
    
    li {{
        margin-bottom: 2px;
    }}
</style>
</head>
<body>
{html_body}
</body>
</html>
"""

with open(html_path, "w", encoding="utf-8") as f:
    f.write(full_html)

print("Generated HTML at:", html_path)

# Run Edge headless to render PDF
edge_path = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
if not os.path.exists(edge_path):
    edge_path = r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"

file_url = "file:///" + html_path.replace("\\", "/")

cmd = [
    edge_path,
    "--headless",
    "--disable-gpu",
    "--run-all-compositor-stages-before-draw",
    f"--print-to-pdf={pdf_path_manuals}",
    file_url
]

print("Executing Edge print-to-pdf...")
result = subprocess.run(cmd, capture_output=True, text=True)
print("Returncode:", result.returncode)

if os.path.exists(pdf_path_manuals):
    size = os.path.getsize(pdf_path_manuals)
    print(f"Success! PDF generated at: {pdf_path_manuals} ({size} bytes)")
    # Also copy to project root
    import shutil
    shutil.copy2(pdf_path_manuals, pdf_path_root)
    print(f"Copied to root: {pdf_path_root}")
else:
    print("Error generating PDF:", result.stderr)
