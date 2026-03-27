# Claude Vision API Tools
Tools and examples for using Claude-3's vision capabilities.

## Installation
`powershell
pip install anthropic
`

## Usage
`python
import anthropic

client = anthropic.Anthropic(api_key="your-api-key")
message = client.messages.create(
    model="claude-3-sonnet-20240229",
    max_tokens=1024,
    messages=[
        {
            "role": "user",
            "content": [
                {
                    "type": "image",
                    "source": {
                        "type": "base64",
                        "media_type": "image/jpeg",
                        "data": "base64-encoded-image"
                    }
                },
                {
                    "type": "text", 
                    "text": "Describe this image"
                }
            ]
        }
    ]
)
`
