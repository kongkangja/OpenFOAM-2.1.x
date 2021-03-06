/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "extendedFaceToCellStencil.H"

// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class Type>
void Foam::extendedFaceToCellStencil::collectData
(
    const mapDistribute& map,
    const labelListList& stencil,
    const GeometricField<Type, fvsPatchField, surfaceMesh>& fld,
    List<List<Type> >& stencilFld
)
{
    // 1. Construct face data in compact addressing
    List<Type> compactFld(map.constructSize(), pTraits<Type>::zero);

    // Insert my internal values
    forAll(fld, cellI)
    {
        compactFld[cellI] = fld[cellI];
    }
    // Insert my boundary values
    label nCompact = fld.size();
    forAll(fld.boundaryField(), patchI)
    {
        const fvsPatchField<Type>& pfld = fld.boundaryField()[patchI];

        forAll(pfld, i)
        {
            compactFld[nCompact++] = pfld[i];
        }
    }

    // Do all swapping
    map.distribute(compactFld);

    // 2. Pull to stencil
    stencilFld.setSize(stencil.size());

    forAll(stencil, faceI)
    {
        const labelList& compactCells = stencil[faceI];

        stencilFld[faceI].setSize(compactCells.size());

        forAll(compactCells, i)
        {
            stencilFld[faceI][i] = compactFld[compactCells[i]];
        }
    }
}


template<class Type>
Foam::tmp<Foam::GeometricField<Type, Foam::fvPatchField, Foam::volMesh> >
Foam::extendedFaceToCellStencil::weightedSum
(
    const mapDistribute& map,
    const labelListList& stencil,
    const GeometricField<Type, fvsPatchField, surfaceMesh>& fld,
    const List<List<scalar> >& stencilWeights
)
{
    const fvMesh& mesh = fld.mesh();

    // Collect internal and boundary values
    List<List<Type> > stencilFld;
    collectData(map, stencil, fld, stencilFld);

    tmp<GeometricField<Type, fvPatchField, volMesh> > tsfCorr
    (
        new GeometricField<Type, fvPatchField, volMesh>
        (
            IOobject
            (
                fld.name(),
                mesh.time().timeName(),
                mesh
            ),
            mesh,
            dimensioned<Type>
            (
                fld.name(),
                fld.dimensions(),
                pTraits<Type>::zero
            )
        )
    );
    GeometricField<Type, fvPatchField, volMesh>& sf = tsfCorr();

    // cells
    forAll(sf, cellI)
    {
        const List<Type>& stField = stencilFld[cellI];
        const List<scalar>& stWeight = stencilWeights[cellI];

        forAll(stField, i)
        {
            sf[cellI] += stField[i]*stWeight[i];
        }
    }

    // Boundaries values?

    return tsfCorr;
}


// ************************************************************************* //
