Add-Type -AssemblyName System.Drawing

function Convert-PngToLvgl {
    param(
        [string]$Png,
        [string]$Out,
        [string]$Symbol,
        [switch]$Alpha
    )

    $bmp = [System.Drawing.Bitmap]::FromFile($Png)
    try {
        $width = $bmp.Width
        $height = $bmp.Height
        $bytes = New-Object System.Collections.Generic.List[byte]

        for ($y = 0; $y -lt $height; $y++) {
            for ($x = 0; $x -lt $width; $x++) {
                $p = $bmp.GetPixel($x, $y)
                $r5 = [int][math]::Round($p.R * 31 / 255)
                $g6 = [int][math]::Round($p.G * 63 / 255)
                $b5 = [int][math]::Round($p.B * 31 / 255)
                $rgb565 = ($r5 -shl 11) -bor ($g6 -shl 5) -bor $b5
                $bytes.Add([byte]($rgb565 -band 0xFF))
                $bytes.Add([byte](($rgb565 -shr 8) -band 0xFF))
                if ($Alpha) { $bytes.Add($p.A) }
            }
        }

        $cf = 'LV_IMG_CF_TRUE_COLOR'
        if ($Alpha) { $cf = 'LV_IMG_CF_TRUE_COLOR_ALPHA' }
        $dataVar = "${Symbol}_data"

        $hexLines = @()
        for ($i = 0; $i -lt $bytes.Count; $i += 12) {
            $slice = $bytes[$i..([Math]::Min($i + 11, $bytes.Count - 1))] | ForEach-Object { "0x{0:X2}" -f $_ }
            $hexLines += ($slice -join ",")
        }

        $body = @()
        $body += '#include "ui.h"'
        $body += ''
        $body += '#ifndef LV_ATTRIBUTE_MEM_ALIGN'
        $body += '#define LV_ATTRIBUTE_MEM_ALIGN'
        $body += '#endif'
        $body += ''
        $body += "const LV_ATTRIBUTE_MEM_ALIGN uint8_t $dataVar[] = {"
        $body += ($hexLines -join ",`n")
        $body += "};"
        $body += ''
        $body += "const lv_img_dsc_t $Symbol = {"
        $body += '  .header.always_zero = 0,'
        $body += "  .header.w = $width,"
        $body += "  .header.h = $height,"
        $body += "  .data_size = sizeof($dataVar),"
        $body += "  .header.cf = $cf,"
        $body += "  .data = $dataVar};"
        $body += ''

        Set-Content -Path $Out -Value $body -Encoding ASCII
        Write-Host "Converted $Png -> $Out ($width x $height, alpha=$($Alpha.IsPresent))"
    }
    finally {
        $bmp.Dispose()
    }
}

Convert-PngToLvgl -Png '../assistant_page4_clean_assets/back4_full_original.png' -Out 'ui_img_s4_back4_png.c' -Symbol 'ui_img_s4_back4_png'
Convert-PngToLvgl -Png '../assistant_page4_clean_assets/mode1_full_no_text.png' -Out 'ui_img_s4_mode1_png.c' -Symbol 'ui_img_s4_mode1_png' -Alpha
Convert-PngToLvgl -Png '../assistant_page4_clean_assets/mode2_full_no_text.png' -Out 'ui_img_s4_mode2_png.c' -Symbol 'ui_img_s4_mode2_png' -Alpha
Convert-PngToLvgl -Png '../assistant_page4_clean_assets/page_dot_on.png' -Out 'ui_img_s4_cut4_1_png.c' -Symbol 'ui_img_s4_cut4_1_png' -Alpha
Convert-PngToLvgl -Png '../assistant_page4_clean_assets/page_dot_off.png' -Out 'ui_img_s4_cut4_2_png.c' -Symbol 'ui_img_s4_cut4_2_png' -Alpha
Convert-PngToLvgl -Png '../assistant_page4_clean_assets/card1_icon.png' -Out 'ui_img_s4_card1_png.c' -Symbol 'ui_img_s4_card1_png' -Alpha
Convert-PngToLvgl -Png '../assistant_page4_clean_assets/card2_icon.png' -Out 'ui_img_s4_card2_png.c' -Symbol 'ui_img_s4_card2_png' -Alpha
Convert-PngToLvgl -Png '../assistant_page4_clean_assets/card3_icon.png' -Out 'ui_img_s4_card3_png.c' -Symbol 'ui_img_s4_card3_png' -Alpha
Convert-PngToLvgl -Png '../assistant_page4_clean_assets/card4_icon.png' -Out 'ui_img_s4_card4_png.c' -Symbol 'ui_img_s4_card4_png' -Alpha
