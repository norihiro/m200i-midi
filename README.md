# M-200i USB MIDI daemon

# Usage
## Starting the daemon

For example, listening TCP port 12345 for text control,
m200i-midi -p 12345 -u /dev/midi0

## Using TCP text control
### List device names
```
list
```

### Send data
```
dt address data...
```

### Request data
```
rq address size
```

### Read back data
```
rb address size
```
