from pathlib import Path

ASSETS = {
    "www/assets/home-grid.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="g1" x1="0%" x2="100%" y1="0%" y2="100%">
      <stop offset="0%" stop-color="#60a5fa" />
      <stop offset="100%" stop-color="#0ea5e9" />
    </linearGradient>
  </defs>
  <rect width="400" height="260" rx="24" fill="url(#g1)" />
  <circle cx="90" cy="130" r="40" fill="rgba(255,255,255,0.35)" />
  <circle cx="310" cy="80" r="60" fill="rgba(255,255,255,0.2)" />
  <rect x="50" y="180" width="220" height="40" rx="12" fill="rgba(15,23,42,0.2)" />
</svg>
""",
    "www/assets/home-global.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="g2" x1="0%" x2="0%" y1="0%" y2="100%">
      <stop offset="0%" stop-color="#c084fc" />
      <stop offset="100%" stop-color="#7c3aed" />
    </linearGradient>
  </defs>
  <rect width="400" height="260" rx="24" fill="url(#g2)" />
  <circle cx="160" cy="90" r="80" fill="rgba(255,255,255,0.18)" />
  <rect x="60" y="150" width="280" height="30" rx="10" fill="rgba(255,255,255,0.4)" />
  <rect x="120" y="190" width="160" height="40" rx="10" fill="rgba(255,255,255,0.25)" />
</svg>
""",
    "www/assets/services-ops.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="g3" x1="0%" x2="100%" y1="0%" y2="0%">
      <stop offset="0%" stop-color="#f97316" />
      <stop offset="100%" stop-color="#facc15" />
    </linearGradient>
  </defs>
  <rect width="400" height="260" rx="24" fill="url(#g3)" />
  <rect x="50" y="50" width="300" height="180" rx="20" fill="rgba(2,6,23,0.2)" />
  <line x1="80" y1="100" x2="320" y2="100" stroke="rgba(255,255,255,0.6)" stroke-width="5" />
  <line x1="80" y1="140" x2="320" y2="140" stroke="rgba(255,255,255,0.4)" stroke-width="5" />
</svg>
""",
    "www/assets/services-quantum.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <radialGradient id="g4" cx="50%" cy="30%" r="80%">
      <stop offset="0%" stop-color="#38bdf8" />
      <stop offset="100%" stop-color="#0ea5e9" />
    </radialGradient>
  </defs>
  <rect width="400" height="260" rx="24" fill="url(#g4)" />
  <path d="M80 180 Q200 50 320 180" stroke="rgba(255,255,255,0.7)" stroke-width="8" fill="none" />
  <circle cx="200" cy="110" r="40" fill="rgba(255,255,255,0.25)" />
</svg>
""",
    "www/assets/home-health.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="g5" x1="0%" x2="100%" y1="0%" y2="100%">
      <stop offset="0%" stop-color="#fb7185" />
      <stop offset="100%" stop-color="#f97316" />
    </linearGradient>
  </defs>
  <rect width="400" height="260" rx="24" fill="url(#g5)" />
  <circle cx="90" cy="90" r="45" fill="rgba(255,255,255,0.4)" />
  <rect x="150" y="150" width="160" height="20" rx="10" fill="rgba(15,23,42,0.25)" />
  <rect x="110" y="190" width="220" height="20" rx="10" fill="rgba(15,23,42,0.2)" />
</svg>
""",
    "www/assets/services-api.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <rect width="400" height="260" rx="24" fill="#0f172a" />
  <g fill="none" stroke="rgba(59,130,246,0.5)" stroke-width="8">
    <circle cx="200" cy="130" r="70" />
    <circle cx="200" cy="130" r="30" />
  </g>
  <line x1="80" y1="130" x2="320" y2="130" stroke="rgba(14,165,233,0.4)" stroke-width="6" />
</svg>
""",
    "www/assets/matrix-physics.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <linearGradient id="g7" x1="0%" x2="100%" y1="0%" y2="100%">
    <stop offset="0%" stop-color="#22d3ee" />
    <stop offset="100%" stop-color="#0ea5e9" />
  </linearGradient>
  <rect width="400" height="260" rx="24" fill="url(#g7)" />
  <rect x="60" y="70" width="280" height="120" rx="24" fill="rgba(15,23,42,0.2)" />
  <circle cx="200" cy="130" r="60" fill="rgba(255,255,255,0.25)" />
</svg>
""",
    "www/assets/insights-telemetry.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <rect width="400" height="260" rx="24" fill="#0f172a" />
  <polyline points="40,220 120,150 200,170 280,130 360,160" fill="none" stroke="#a855f7" stroke-width="12" stroke-linecap="round" />
  <circle cx="120" cy="150" r="14" fill="#a855f7" />
  <circle cx="280" cy="130" r="14" fill="#a855f7" />
</svg>
""",
    "www/assets/insights-subqg.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <radialGradient id="g8" cx="50%" cy="50%" r="70%">
      <stop offset="0%" stop-color="#fde047" />
      <stop offset="100%" stop-color="#facc15" />
    </radialGradient>
  </defs>
  <rect width="400" height="260" rx="24" fill="url(#g8)" />
  <path d="M60 200 C160 120 240 180 360 110" stroke="#0f172a" stroke-width="12" fill="none" />
  <circle cx="360" cy="110" r="14" fill="#0f172a" />
</svg>
""",
    "www/assets/matrix-grid.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <rect width="400" height="260" rx="24" fill="#0f172a" />
  <g stroke="rgba(59,130,246,0.5)" stroke-width="3">
    <line x1="40" y1="40" x2="360" y2="40" />
    <line x1="40" y1="140" x2="360" y2="140" />
    <line x1="40" y1="240" x2="360" y2="240" />
  </g>
  <g fill="#38bdf8">
    <circle cx="120" cy="90" r="14" />
    <circle cx="200" cy="200" r="16" />
    <circle cx="280" cy="70" r="12" />
  </g>
</svg>
""",
    "www/assets/matrix-ai.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <rect width="400" height="260" rx="24" fill="#111827" />
  <circle cx="200" cy="130" r="70" fill="rgba(59,130,246,0.3)" />
  <circle cx="200" cy="90" r="20" fill="rgba(14,165,233,0.9)" />
  <circle cx="160" cy="160" r="10" fill="rgba(14,165,233,0.7)" />
  <circle cx="240" cy="160" r="10" fill="rgba(14,165,233,0.7)" />
</svg>
""",
    "www/assets/insights-forecast.svg": """<svg width="400" height="260" xmlns="http://www.w3.org/2000/svg">
  <rect width="400" height="260" rx="24" fill="url(#g6)" />
  <defs>
    <linearGradient id="g6" x1="0%" x2="100%" y1="0%" y2="100%">
      <stop offset="0%" stop-color="#a3e635" />
      <stop offset="100%" stop-color="#65a30d" />
    </linearGradient>
  </defs>
  <polyline points="40,210 120,150 200,170 280,120 360,140" fill="none" stroke="#111827" stroke-width="10" stroke-linecap="round" />
  <circle cx="360" cy="140" r="12" fill="#111827" />
</svg>
""",
}


def main() -> None:
    for path, content in ASSETS.items():
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content)


if __name__ == "__main__":
    main()
