"""*WIKI* 

Stitches single histogram [[MatrixWorkspace|Matrix Workspaces]] together outputing a stitched Matrix Workspace. This algorithm is a wrapper over [[Stitch1DMD]].

*WIKI*"""
from mantid.simpleapi import *

from mantid.api import *
from mantid.kernel import *
import numpy as np

class Stitch1D(PythonAlgorithm):

    def category(self):
        return "Reflectometry\\ISIS;PythonAlgorithms"

    def name(self):
	    return "Stitch1D"

    def PyInit(self):
        self.declareProperty(MatrixWorkspaceProperty("LHSWorkspace", "", Direction.Input), "Input workspace")
        self.declareProperty(MatrixWorkspaceProperty("RHSWorkspace", "", Direction.Input), "Input workspace")
        self.declareProperty(MatrixWorkspaceProperty("OutputWorkspace", "", Direction.Output), "Output stitched workspace")
        
        overlap_validator = FloatMandatoryValidator() 

        self.declareProperty(name="StartOverlap", defaultValue=-1.0, validator=overlap_validator, doc="Overlap in Q.")
        self.declareProperty(name="EndOverlap", defaultValue=-1.0, validator=overlap_validator, doc="End overlap in Q.")
        self.declareProperty(name="Params", defaultValue="0.1", doc="Rebinning Parameters. See Rebin for format.")
        self.declareProperty(name="ScaleRHSWorkspace", defaultValue=True, doc="Scaling either with respect to workspace 1 or workspace 2.")
        self.declareProperty(name="UseManualScaleFactor", defaultValue=False, doc="True to use a provided value for the scale factor.")
        self.declareProperty(name="ManualScaleFactor", defaultValue=1.0, doc="Provided value for the scale factor.")
        self.declareProperty(name="OutScaleFactor", defaultValue=-2.0, direction = Direction.Output, doc="The actual used value for the scaling factor.")
    
    def has_non_zero_errors(self, ws):
        errors = ws.extractE()
        count = np.count_nonzero(errors)
        return count > 0
            
    def PyExec(self):
        # Just forward the other properties on.
        startOverlap = self.getProperty('StartOverlap').value
        endOverlap = self.getProperty('EndOverlap').value
        scaleRHSWorkspace = self.getProperty('ScaleRHSWorkspace').value
        useManualScaleFactor = self.getProperty('UseManualScaleFactor').value
        manualScaleFactor = self.getProperty('ManualScaleFactor').value
        outScaleFactor = self.getProperty('OutScaleFactor').value
        params = self.getProperty("Params").value
        
        lhs_rebinned = Rebin(InputWorkspace=self.getProperty("LHSWorkspace").value, Params=params)
        rhs_rebinned = Rebin(InputWorkspace=self.getProperty("RHSWorkspace").value, Params=params)
        
        a1=lhs_rebinned.binIndexOf(startOverlap)
        a2=lhs_rebinned.binIndexOf(endOverlap)
    
        if not useManualScaleFactor:
            lhsOverlapIntegrated = Integration(InputWorkspace=lhs_rebinned, RangeLower=startOverlap, RangeUpper=endOverlap)
            rhsOverlapIntegrated = Integration(InputWorkspace=rhs_rebinned, RangeLower=startOverlap, RangeUpper=endOverlap)
            y1=lhsOverlapIntegrated.readY(0)
            y2=rhsOverlapIntegrated.readY(0)
            if scaleRHSWorkspace:
                rhs_rebinned *= (lhsOverlapIntegrated/rhsOverlapIntegrated)
                scalefactor = y1[0]/y2[0]
            else:
                lhs_rebinned *= (rhsOverlapIntegrated/lhsOverlapIntegrated)
                scalefactor = y2[0]/y1[0]
            
        else:
            rhs_rebinned = MultiplyRange(InputWorkspace=w2, StartBin=0, Factor=manualScaleFactor)
            scalefactor = manualScaleFactor
        
        # Mask out everything BUT the overlap region as a new workspace.
        overlap1 = MultiplyRange(InputWorkspace=lhs_rebinned, StartBin=0,EndBin=a1,Factor=0)
        overlap1 = MultiplyRange(InputWorkspace=overlap1,StartBin=a2,Factor=0)
    
        # Mask out everything BUT the overlap region as a new workspace.
        overlap2 = MultiplyRange(InputWorkspace=rhs_rebinned,StartBin=0,EndBin=a1,Factor=0)#-1
        overlap2 = MultiplyRange(InputWorkspace=overlap2,StartBin=a2,Factor=0)
    
        # Mask out everything AFTER the start of the overlap region
        lhs_rebinned=MultiplyRange(InputWorkspace=lhs_rebinned, StartBin=a1+1, Factor=0)
        # Mask out everything BEFORE the end of the overlap region
        rhs_rebinned=MultiplyRange(InputWorkspace=rhs_rebinned,StartBin=0,EndBin=a2-1,Factor=0)
        
        # Calculate a weighted mean for the overlap region
        overlapave = None
        if self.has_non_zero_errors(overlap1) and self.has_non_zero_errors(overlap2):
            overlapave = WeightedMean(InputWorkspace1=overlap1,InputWorkspace2=overlap2)
        else:
            overlapave = (overlap1 + overlap2)/2
            
        # Add the Three masked workspaces together to create a complete x-range
        result = lhs_rebinned + overlapave + rhs_rebinned
        RenameWorkspace(InputWorkspace=result, OutputWorkspace=self.getPropertyValue("OutputWorkspace"))
        
        # Cleanup
        DeleteWorkspace(lhs_rebinned)
        DeleteWorkspace(rhs_rebinned)
        DeleteWorkspace(lhsOverlapIntegrated)
        DeleteWorkspace(rhsOverlapIntegrated)
        DeleteWorkspace(overlap1)
        DeleteWorkspace(overlap2)
        DeleteWorkspace(overlapave)
       
        self.setProperty('OutputWorkspace', result)
        self.setProperty('OutScaleFactor', scalefactor)
        
        return None
        

#############################################################################################

AlgorithmFactory.subscribe(Stitch1D())
