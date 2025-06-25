class gameObject:
    def __init__(self, sprite, position = (0,0,0)):
        self.sprite = sprite
        self.transform = transform(position=position)
        self.collider = transform.collider
class transform:
    def __init__(self, position = (0,0,0), rotation = (0,0,0), scale = (1,1,1)):
        self.position = position
        self.rotation = rotation
        self.scale = scale
        self.collider = collider(position, rotation, scale)
class collider:
    def __init__(self, position, rotation, scale):
        self.position = position
        self.onCollision = None
        self.width = 0
        self.height = 0
        self.depth = 0
    def widthInLine(p1,w1,p2,w2):
         if(p2 + w2 > p1 and p2 < p1 + w1):
            return True
         return False
    def heightInLine(p1,h1,p2,h2):
         if(p2 + h2 > p1 and p2 < p1 + h1):
            return True
         return False
    def depthInLine(p1,d1,p2,d2):
         if(p2 + d2 > p1 and p2 < p1 + d1):
            return True
         return False
    def collide(self, other):
        if self.widthInLine(self.position[0], self.width, other.position[0], other.width) and self.heightInLine(self.position[1], self.height, other.position[1], other.height) and self.depthInLine(self.position[2], self.depth, other.position[2], other.depth):
            if self.onCollision != None:
                self.onCollision(self,other)
            return other
        return None
