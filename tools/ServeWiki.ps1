param([int]$Port = 8777)
# Derived from the script's own location, not hard-coded. The absolute path that used to sit here
# carried the repo's folder name, so renaming the project (V1 dropped 2026-08-31) would have left
# this serving a folder that no longer exists - and it would have failed at request time, not at
# start-up, which is the worst place to learn it.
$root = Join-Path (Split-Path -Parent $PSScriptRoot) 'wiki'
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:$Port/")
$listener.Start()
Write-Host "serving $root on http://localhost:$Port/"
while ($listener.IsListening) {
    $ctx = $listener.GetContext()
    $rel = [System.Uri]::UnescapeDataString($ctx.Request.Url.AbsolutePath.TrimStart('/'))
    if ($rel -eq '') { $rel = 'index.html' }
    $path = Join-Path $root $rel
    if (Test-Path $path -PathType Leaf) {
        $bytes = [System.IO.File]::ReadAllBytes($path)
        $ext = [System.IO.Path]::GetExtension($path).ToLower()
        $type = 'text/plain'
        if ($ext -eq '.html') { $type = 'text/html; charset=utf-8' }
        elseif ($ext -eq '.css') { $type = 'text/css; charset=utf-8' }
        elseif ($ext -eq '.js') { $type = 'application/javascript; charset=utf-8' }
        elseif ($ext -eq '.png') { $type = 'image/png' }
        $ctx.Response.ContentType = $type
        $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
    } else {
        $ctx.Response.StatusCode = 404
    }
    $ctx.Response.Close()
}
