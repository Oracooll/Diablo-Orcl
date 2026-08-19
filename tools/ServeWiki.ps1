param([int]$Port = 8777)
$root = "C:\Users\hroga\OneDrive\2. Personal Files\Software\Diablo\Diablo Orcl V1\wiki"
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
