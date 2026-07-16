# Firemod capability contracts

Installed suite components may place reviewed `.capability` files here, in
`$FIREMOD_CAPABILITY_DIR`, or in an XDG data directory beneath
`firemod/capabilities.d`.

```ini
[capability]
contract-version=1
id=capture.screenshot.output
owner=Fireshit Capture
label=Screenshot
icon=camera-photo-symbolic
menu-visible=true
order=10
executable=fireshit-capture
command=fireshit-capture screenshot-output --at {x} {y}
```

`command` is parsed into an argument vector and never passed through a shell.
Only `{x}` and `{y}` invocation-coordinate substitutions are admitted.

Discovery does not create executable actions. A missing or mismatched contract
version, absent `menu-visible=true`, missing required fields, or unavailable
owner executable leaves the claim outside the executable global menu.
