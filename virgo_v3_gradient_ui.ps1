
$ErrorActionPreference = 'Stop'

function Read-TextLF([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Required file not found: $Path"
    }

    $value = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $Path))
    return [regex]::Replace($value, '\r\n?', [string][char]10)
}

function Write-TextLF([string]$Path, [string]$Content) {
    [IO.File]::WriteAllText(
        [IO.Path]::GetFullPath($Path),
        $Content,
        [Text.UTF8Encoding]::new($false)
    )
}

function Replace-Function(
    [string]$Source,
    [string]$Signature,
    [string]$Replacement,
    [string]$Label
) {
    $count = ([regex]::Matches(
        $Source,
        [regex]::Escape($Signature)
    )).Count

    if ($count -ne 1) {
        throw "Expected one $Label signature; found $count. No files written."
    }

    $start = $Source.IndexOf($Signature, [StringComparison]::Ordinal)
    $open = $Source.IndexOf('{', $start)

    if ($open -lt 0) {
        throw "Missing opening brace for $Label."
    }

    $depth = 0
    $end = -1

    for ($i = $open; $i -lt $Source.Length; $i++) {
        if ($Source[$i] -eq '{') {
            $depth++
        }
        elseif ($Source[$i] -eq '}') {
            $depth--

            if ($depth -eq 0) {
                $end = $i + 1
                break
            }
        }
    }

    if ($end -lt 0) {
        throw "Missing closing brace for $Label."
    }

    return $Source.Substring(0, $start) +
        $Replacement.TrimEnd() +
        $Source.Substring($end)
}

$path = 'src/FloweristUI.cpp'
$source = Read-TextLF $path

if ($source.Contains('// VIRGO V3 GRADIENT CARD SYSTEM')) {
    Write-Host 'VIRGO V3 gradient UI is already applied.'
    exit 0
}

$card = @'
// VIRGO V3 GRADIENT CARD SYSTEM
void card(CCNode* parent, float x, float y, float w, float h,
          ccColor4B fill = rgba(20, 25, 49, 250)) {
    (void)fill;

    roundedRect(parent, x + 3.f, y - 4.f, w, h, 14.f,
                ccc4f(.002f, .004f, .018f, .78f), 1);

    roundedRect(parent, x, y, w, h, 14.f,
                ccc4f(.10f, .13f, .25f, .98f), 2);

    roundedRect(parent, x + 1.5f, y + 1.5f, w - 3.f, h - 3.f, 12.5f,
                ccc4f(.035f, .045f, .095f, .995f), 3);

    roundedRect(parent, x + 3.f, y + 3.f, w - 6.f, h - 6.f, 10.5f,
                ccc4f(.050f, .055f, .105f, .995f), 4);

    // Cyan -> Blue -> Violet -> Pink gradient accent.
    auto edge = CCDrawNode::create();

    if (edge) {
        const ccColor4F colors[] = {
            ccc4f(.12f, .82f, .96f, .96f),
            ccc4f(.22f, .55f, 1.f, .96f),
            ccc4f(.48f, .38f, 1.f, .96f),
            ccc4f(.78f, .32f, .91f, .96f),
            ccc4f(1.f, .38f, .72f, .96f)
        };

        const float left = x + 15.f;
        const float right = x + w - 15.f;
        const float top = y + h - 5.f;

        for (int i = 0; i < 5; ++i) {
            const float a = left + (right - left) *
                (static_cast<float>(i) / 5.f);

            const float b = left + (right - left) *
                (static_cast<float>(i + 1) / 5.f);

            edge->drawSegment(
                {a, top},
                {b, top},
                1.35f,
                colors[i]
            );
        }

        edge->drawSegment(
            {left, top - 2.5f},
            {right, top - 2.5f},
            .55f,
            ccc4f(.48f, .52f, 1.f, .20f)
        );

        parent->addChild(edge, 5);
    }
}
'@

$source = Replace-Function `
    $source `
    'void card(CCNode* parent, float x, float y, float w, float h,' `
    $card `
    'card()'

Write-TextLF $path $source

Write-Host 'VIRGO V3 gradient card system applied.'
